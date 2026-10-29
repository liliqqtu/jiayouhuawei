/**
 * 全局语音播报（TTS）管理器。
 *
 * 为什么做成单例：CoreSpeechKit 限制「同一台设备上所有 App 最多 3 个 TTS 实例」，
 * 每个页面各 createEngine 一次很快就会把名额耗光（后续 init 全部失败）。
 *
 * 三条安全约束（本项目硬要求）：
 *   1. 一切异常都不许外抛：引擎不存在 / 创建失败 / 播报失败，一律只记日志并静默降级，
 *      绝不能因为"念不出来"让告警流程崩溃或 APP 闪退；
 *   2. 模拟器一般没有 TTS 引擎，createEngine 会 reject —— 这是预期路径，不算错误；
 *   3. destroy() 必须配对调用，否则实例泄漏。
 *
 * 播报策略：
 *   - 紧急（L3 / isUrgent=true）：queueMode=0，立刻打断正在念的内容；
 *   - 普通（L1/L2）：queueMode=1，排队，等前一句念完，避免互相插嘴。
 */
import { textToSpeech } from '@kit.CoreSpeechKit';
import { BusinessError } from '@kit.BasicServicesKit';

const TAG: string = '[TtsManager]';

export class TtsManager {
  private static instance: TtsManager | null = null;

  /** 单例入口：全 APP 只允许一个引擎 */
  static getInstance(): TtsManager {
    if (TtsManager.instance === null) {
      TtsManager.instance = new TtsManager();
    }
    return TtsManager.instance;
  }

  private engine: textToSpeech.TextToSpeechEngine | null = null;
  /** 引擎是否可用；false 时 speak() 直接返回，不抛异常 */
  private ready: boolean = false;
  /** 是否已经尝试过初始化（失败后不再反复创建，避免刷日志和耗实例名额） */
  private tried: boolean = false;
  /** requestId 自增，便于在日志里对上 onStart/onComplete/onError */
  private seq: number = 0;

  /**
   * 初始化引擎。可重复调用，内部只真正执行一次。
   * @returns 是否可用（调用方一般不需要关心，失败会自动静默降级）
   */
  async init(): Promise<boolean> {
    if (this.tried) {
      return this.ready;
    }
    this.tried = true;
    try {
      const params: textToSpeech.CreateEngineParams = {
        language: 'zh-CN',
        // 中文女声；21 为男声（需先下载音色），13 为离线自带，最稳
        person: 13,
        // 1=离线模式（0=在线模式当前不支持），离线才不依赖网络
        online: 1,
        extraParams: {
          'style': 'interaction-broadcast',
          'locate': 'CN',
          'name': 'shadowlamp-tts',
          'isBackStage': true
        }
      };
      this.engine = await textToSpeech.createEngine(params);
      this.engine.setListener({
        onStart: (requestId: string): void => {
          console.info(`${TAG} onStart ${requestId}`);
        },
        onComplete: (requestId: string): void => {
          console.info(`${TAG} onComplete ${requestId}`);
        },
        onStop: (requestId: string): void => {
          console.info(`${TAG} onStop ${requestId}`);
        },
        onError: (requestId: string, errorCode: number, errorMessage: string): void => {
          // 播报失败只记日志：不弹窗打扰用户，也不影响告警入库/展示
          console.error(`${TAG} onError requestId=${requestId} code=${errorCode} msg=${errorMessage}`);
        }
      });
      this.ready = true;
      console.info(`${TAG} init ok (offline zh-CN person=13)`);
      return true;
    } catch (err) {
      const e = err as BusinessError;
      // 模拟器 / 无引擎设备走这里：预期路径，静默降级
      this.ready = false;
      this.engine = null;
      console.warn(`${TAG} init failed, 降级为静音：code=${e.code} msg=${e.message}`);
      return false;
    }
  }

  /**
   * 播报一句话。
   * @param text 要念的内容（建议 ≤60 字，老人记不住长句）
   * @param isUrgent true=L3 紧急（打断当前播报），false=普通（排队）
   */
  speak(text: string, isUrgent: boolean): void {
    if (!this.ready || this.engine === null) {
      console.info(`${TAG} speak skipped (engine unavailable): ${text}`);
      return;
    }
    if (text === null || text === undefined || text.length === 0) {
      return;
    }
    this.seq = this.seq + 1;
    const requestId: string = `sl${this.seq}`;
    try {
      this.engine.speak(text, {
        requestId: requestId,
        extraParams: {
          // 0=打断当前播报（紧急优先）；1=排队等前一句念完
          'queueMode': isUrgent ? 0 : 1,
          // 老人场景语速略慢、音量正常：太快等于没念
          'speed': 0.9,
          'volume': 1.0,
          'pitch': 1.0,
          'style': 'interaction-broadcast',
          'locate': 'CN'
        }
      });
      console.info(`${TAG} speak ok requestId=${requestId} urgent=${isUrgent} text=${text}`);
    } catch (err) {
      const e = err as BusinessError;
      console.error(`${TAG} speak failed: code=${e.code} msg=${e.message}`);
    }
  }

  /** 停止当前播报（用户主动关闭语音、或进入静音场景时调用） */
  stop(): void {
    if (this.engine === null) {
      return;
    }
    try {
      this.engine.stop();
      console.info(`${TAG} stop ok`);
    } catch (err) {
      const e = err as BusinessError;
      console.warn(`${TAG} stop failed: code=${e.code} msg=${e.message}`);
    }
  }

  /** 是否可用（UI 可据此把"语音播报"开关置灰/提示） */
  isReady(): boolean {
    return this.ready;
  }

  /** 释放引擎；必须与 init() 配对，否则一直占着实例名额 */
  destroy(): void {
    if (this.engine === null) {
      this.ready = false;
      return;
    }
    try {
      this.engine.shutdown();
      console.info(`${TAG} destroy ok`);
    } catch (err) {
      const e = err as BusinessError;
      console.warn(`${TAG} destroy failed: code=${e.code} msg=${e.message}`);
    }
    this.engine = null;
    this.ready = false;
    this.tried = false;
  }
}
