/**
 * 系统通知栏提醒。
 *
 * 为什么要它：原来只有本机震动 + 提示音，APP 退到后台、手机扣在桌上时家属收不到；
 * 告警响铃会被系统静音策略吞掉，而通知栏条目会留在屏幕上方，事后能回看。
 *
 * 能力边界（写清楚，免得演示时误判）：
 * - 这是本地通知（notificationManager.publish），不需要推送服务，也不需要服务端。
 * - 通知只在 APP 进程还活着、3 秒轮询还在跑时产生；进程被系统冻结后不会再弹。
 * - 首次使用会弹系统授权框，用户拒绝就静默降级（震动 + 提示音仍然有效）。
 */
import { common } from '@kit.AbilityKit';
import { wantAgent, WantAgent } from '@kit.AbilityKit';
import { notificationManager } from '@kit.NotificationKit';
// NotificationRequest.slotType 用的是旧的 @ohos.notification 枚举（SDK 内部历史原因），
// 与 notificationManager.SlotType 不是同一个类型，混用会编译报错，故两个都引入。
import notification from '@ohos.notification';

export class Notifier {
  private static ctx: common.UIAbilityContext | null = null;
  /** 系统通知开关是否可用（用户拒绝授权时置 false，后续直接跳过，不刷日志） */
  private static enabled: boolean = false;
  /** 通知 id 递增：每条告警一个独立条目，而不是互相覆盖 */
  private static nextId: number = 2000;
  /** 同一个事件 20 秒内只弹一次，避免 3 秒轮询把同一条告警刷成 6 条 */
  private static lastKey: string = '';
  private static lastAt: number = 0;

  /** 初始化：申请授权、建立高优先级通知通道 */
  static async init(ctx: common.UIAbilityContext): Promise<void> {
    Notifier.ctx = ctx;
    try {
      Notifier.enabled = await notificationManager.isNotificationEnabled();
      if (!Notifier.enabled) {
        // 弹系统授权框；用户点“允许”后重新查一次状态
        await notificationManager.requestEnableNotification(ctx);
        Notifier.enabled = await notificationManager.isNotificationEnabled();
      }
      if (Notifier.enabled) {
        await Notifier.ensureSlot();
      }
    } catch (err) {
      Notifier.enabled = false;
      console.warn('[Notifier] 通知不可用，降级为只震动：' + JSON.stringify(err));
    }
  }

  /**
   * 建立“家庭安全”通知通道。
   * SOCIAL_COMMUNICATION 这个类型本身对应 LEVEL_HIGH：状态栏图标 + 顶部横幅 + 声音，
   * 正好是“手机上方的通知”要的效果；通道已存在时重复 addSlot 是幂等的。
   */
  private static async ensureSlot(): Promise<void> {
    try {
      await notificationManager.addSlot(notificationManager.SlotType.SOCIAL_COMMUNICATION);
    } catch (err) {
      console.warn('[Notifier] 通知通道创建失败（仍可发通知）：' + JSON.stringify(err));
    }
  }

  /**
   * 弹一条告警通知。
   * level 传 L1/L2/L3，只影响标题文案与是否强调，不改变通知本身的能力。
   */
  static async alarm(level: string, message: string, eventId: string): Promise<void> {
    if (!Notifier.enabled || Notifier.ctx === null) {
      return;
    }
    const key: string = eventId.length > 0 ? eventId : message;
    const now: number = Date.now();
    if (key === Notifier.lastKey && now - Notifier.lastAt < 20000) {
      return;
    }
    Notifier.lastKey = key;
    Notifier.lastAt = now;

    const levelText: string = level === 'L3' ? '紧急告警' : (level === 'L2' ? '重要告警' : '提醒');
    const text: string = message.length > 0 ? message : '检测到异常，请打开守夜灯查看';
    const request: notificationManager.NotificationRequest = {
      id: Notifier.nextId,
      content: {
        notificationContentType: notificationManager.ContentType.NOTIFICATION_CONTENT_BASIC_TEXT,
        normal: {
          title: '守夜灯 · ' + levelText,
          text: text,
          additionalText: '点击查看详情并处置'
        }
      },
      slotType: notification.SlotType.SOCIAL_COMMUNICATION,
      extraInfo: { level: level }
    };
    const agent: WantAgent | null = await Notifier.buildWantAgent();
    if (agent !== null) {
      // 点通知回到 APP：用操作类型 START_ABILITY 打开自己的入口 Ability
      request.wantAgent = agent;
    }
    try {
      await notificationManager.publish(request);
      Notifier.nextId += 1;
    } catch (err) {
      console.warn('[Notifier] 通知发送失败：' + JSON.stringify(err));
    }
  }

  /** 没有待确认告警时清空通知栏，避免“事情早处理完了通知还挂着” */
  static async cancelAll(): Promise<void> {
    if (!Notifier.enabled) {
      return;
    }
    try {
      await notificationManager.cancelAll();
    } catch (err) {
      console.warn('[Notifier] 清空通知失败：' + JSON.stringify(err));
    }
  }

  /** 构造“点击通知回到 APP”的 WantAgent；失败返回 null，不影响通知本身 */
  private static async buildWantAgent(): Promise<WantAgent | null> {
    const ctx: common.UIAbilityContext | null = Notifier.ctx;
    if (ctx === null) {
      return null;
    }
    try {
      const info: wantAgent.WantAgentInfo = {
        wants: [{
          bundleName: ctx.abilityInfo.bundleName,
          abilityName: ctx.abilityInfo.name
        }],
        actionType: wantAgent.OperationType.START_ABILITY,
        requestCode: 0,
        actionFlags: [wantAgent.WantAgentFlags.UPDATE_PRESENT_FLAG]
      };
      return await wantAgent.getWantAgent(info);
    } catch (err) {
      console.warn('[Notifier] WantAgent 创建失败：' + JSON.stringify(err));
      return null;
    }
  }
}
