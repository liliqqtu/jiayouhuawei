/**
 * 一键拨号工具。
 *
 * 为什么用 ACTION_DIAL 而不是 @kit.TelephonyKit 的 call.makeCall：
 *   - makeCall 需要 ohos.permission.PLACE_CALL，这是 system 级（系统应用）权限，
 *     三方应用在 module.json5 里声明会申请不到 / 被审核打回，调用必然失败；
 *   - ACTION_DIAL 只是把号码填进系统拨号盘，不需要任何权限，而且要用户再按一次
 *     "呼叫"才真正拨出 —— 对独居老人场景更安全（误触不会直接拨出去）。
 *
 * 安全约束：任何异常都只 toast 提示 + 记日志，绝不外抛导致页面崩溃。
 */
import { common, Want } from '@kit.AbilityKit';
import { BusinessError, pasteboard } from '@kit.BasicServicesKit';
import { promptAction } from '@kit.ArkUI';

const TAG: string = '[CallUtils]';

export class CallUtils {
  /**
   * 拉起系统拨号盘并预填号码。
   * @param context UIAbility 上下文（页面里用 this.getUIContext().getHostContext() 取）
   * @param phoneNumber 电话号码，允许带空格/横线，内部会清洗
   * @returns 是否成功拉起（真实拨出与否由用户在拨号盘决定）
   */
  static async dial(context: common.UIAbilityContext, phoneNumber: string): Promise<boolean> {
    const number: string = CallUtils.normalize(phoneNumber);
    if (number.length === 0) {
      console.warn(`${TAG} 号码为空或格式不对：${phoneNumber}`);
      CallUtils.toast('还没有填家人电话，请先在「我的」页设置');
      return false;
    }
    // 三种隐式拉起方式依次尝试：不同设备/系统版本对 tel: 的注册方式不一样
    // 1) ACTION_DIAL（标准拨号盘）2) ACTION_VIEW_DATA（部分平板走这里）3) openLink（URI 解析）
    const wants: Want[] = [
      { action: 'ohos.want.action.dial', uri: `tel:${number}` },
      { action: 'ohos.want.action.viewData', uri: `tel:${number}` }
    ];
    for (let i = 0; i < wants.length; i++) {
      try {
        await context.startAbility(wants[i]);
        console.info(`${TAG} dial ok via strategy=${i} number=${number}`);
        return true;
      } catch (err) {
        const e = err as BusinessError;
        console.warn(`${TAG} dial strategy=${i} failed: code=${e.code} msg=${e.message}`);
      }
    }
    try {
      await context.openLink(`tel:${number}`);
      console.info(`${TAG} dial ok via openLink number=${number}`);
      return true;
    } catch (err) {
      const e = err as BusinessError;
      console.error(`${TAG} dial failed: all strategies failed, code=${e.code} msg=${e.message} number=${number}`);
    }
    // 退让方案：没有任何拨号应用（模拟器/平板）时，把号码放进剪贴板并交给页面弹窗说明
    await CallUtils.copyOnly(number);
    return false;
  }

  /** 只把号码写进剪贴板（页面弹窗里的“再复制一次”也用它） */
  static async copyOnly(phoneNumber: string): Promise<boolean> {
    const number: string = CallUtils.normalize(phoneNumber);
    if (number.length === 0) {
      return false;
    }
    return CallUtils.copyNumber(number);
  }

  /** 把号码写进剪贴板；失败只记日志，不影响调用方流程 */
  private static async copyNumber(number: string): Promise<boolean> {
    try {
      await pasteboard.getSystemPasteboard().setData(pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, number));
      console.info(`${TAG} number copied: ${number}`);
      return true;
    } catch (err) {
      const e = err as BusinessError;
      console.warn(`${TAG} copy number failed: ${e.message}`);
      return false;
    }
  }

  /** 只保留数字和 +，长度 3~20；避免把奇怪字符串塞给拨号盘 */
  private static normalize(raw: string): string {
    if (raw === null || raw === undefined) {
      return '';
    }
    const digits: string = raw.replace(/[^0-9+]/g, '');
    if (digits.length < 3 || digits.length > 20) {
      return '';
    }
    return digits;
  }

  private static toast(msg: string): void {
    try {
      promptAction.showToast({ message: msg, duration: 2500 });
    } catch (err) {
      const e = err as BusinessError;
      console.error(`${TAG} toast failed: ${e.message}`);
    }
  }
}
