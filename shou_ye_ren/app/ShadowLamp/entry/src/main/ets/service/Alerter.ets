/**
 * 本机告警提醒：震动与提示音
 */
import { common } from '@kit.AbilityKit';
import { media } from '@kit.MediaKit';
import { vibrator } from '@kit.SensorServiceKit';

export class Alerter {
  private static player: media.AVPlayer | null = null;
  private static ready: boolean = false;
  private static inited: boolean = false;
  private static lastLevel: string = '';
  private static lastAt: number = 0;

  /** 初始化：加载 resources/rawfile/alert.wav。没有音频能力时静默降级为只震动 */
  static async init(ctx: common.UIAbilityContext): Promise<void> {
    if (Alerter.inited) {
      return;
    }
    Alerter.inited = true;
    try {
      const raw = await ctx.resourceManager.getRawFd('alert.wav');
      const player = await media.createAVPlayer();
      player.on('stateChange', (state: media.AVPlayerState) => {
        if (state === 'initialized') {
          player.prepare();
        } else if (state === 'prepared') {
          Alerter.ready = true;
        }
      });
      player.fdSrc = { fd: raw.fd, offset: raw.offset, length: raw.length };
      Alerter.player = player;
    } catch (err) {
      // 模拟器 / 无声设备上拿不到音频能力是正常情况，不影响告警本身
      console.warn('[Alerter] 提示音不可用，降级为只震动：' + JSON.stringify(err));
    }
  }

  /**
   * 本机提醒恒为开启：这是安全提醒，不给用户关掉的机会。
   * 保留 setEnabled 只为兼容旧调用点，实际不再改变行为。
   */
  static isEnabled(): boolean {
    return true;
  }

  static setEnabled(_on: boolean): void {
    // 故意空实现：报警类应用不允许把自身提醒关掉
  }

  /**
   * 收到新告警时提醒一次。
   * 降噪：同一等级 15 秒内只响一次，避免 3 秒轮询把提示音刷成噪音。
   */
  static notify(level: string): void {
    if (level.length === 0) {
      return;
    }
    const now: number = Date.now();
    if (Alerter.lastLevel === level && now - Alerter.lastAt < 15000) {
      return;
    }
    Alerter.lastLevel = level;
    Alerter.lastAt = now;
    Alerter.vibrate(level === 'L1' ? 300 : 450);
    Alerter.play();
  }

  private static vibrate(duration: number): void {
    try {
      vibrator.startVibration(
        { type: 'time', duration: duration },
        { id: 0, usage: 'alarm' }
      ).catch((err: Error) => {
        console.warn('[Alerter] 震动不可用：' + err.message);
      });
    } catch (err) {
      console.warn('[Alerter] 震动异常');
    }
  }

  private static play(): void {
    const player = Alerter.player;
    if (player === null || !Alerter.ready) {
      return;
    }
    try {
      player.seek(0);
      player.play();
    } catch (err) {
      console.warn('[Alerter] 播放失败');
    }
  }
}
