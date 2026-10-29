/**
 * 报告长图导出：PixelMap -> PNG -> 相册 / 系统分享。
 *
 * 设计说明（评审/答辩可能会问）：
 * 1. 截图用 componentSnapshot.createFromBuilder 离屏渲染，不依赖“页面当前可见”；
 *    这样长图能一次渲染出标题 + 图表 + 落款，不受滚动视口裁剪。
 * 2. 相册写入走 PhotoAccessHelper：先申请 ohos.permission.WRITE_IMAGEVIDEO，
 *    被拒时退回 showAssetsCreationDialog（由用户在系统弹窗确认，无需权限），
 *    两条路都失败才提示“去设置里开启”。
 * 3. 分享走系统 ShareKit（分享面板由系统绘制），不经过网络，老人数据不出家门。
 */
import { abilityAccessCtrl, common, Permissions, Want } from '@kit.AbilityKit';
import { fileIo, fileUri } from '@kit.CoreFileKit';
import { image } from '@kit.ImageKit';
import { photoAccessHelper } from '@kit.MediaLibraryKit';
import { uniformTypeDescriptor as utd } from '@kit.ArkData';
import { systemShare } from '@kit.ShareKit';

/** 相册保存结果：ok=false 时带原因；needSetting=true 表示应引导去设置页开权限 */
export interface AlbumSaveResult {
  ok: boolean;
  message: string;
  needSetting: boolean;
}

/** 分享结果：只区分“面板已拉起”与“失败原因” */
export interface ShareResult {
  ok: boolean;
  message: string;
}

export class ReportExporter {
  /** PNG 缓存在本应用 cacheDir 下，分享和相册写入都用它 */
  private static pngPath: string = '';

  /** yyyyMMdd_HHmmss，文件名带时间，多次导出不覆盖 */
  static stamp(): string {
    const d: Date = new Date();
    const p2 = (n: number): string => (n < 10 ? '0' + n.toString() : n.toString());
    return d.getFullYear().toString() + p2(d.getMonth() + 1) + p2(d.getDate()) +
      '_' + p2(d.getHours()) + p2(d.getMinutes()) + p2(d.getSeconds());
  }

  /** PixelMap 编码成 PNG 字节 */
  static async toPng(pm: image.PixelMap): Promise<ArrayBuffer> {
    const packer: image.ImagePacker = image.createImagePacker();
    const opt: image.PackingOption = { format: 'image/png', quality: 100 };
    const buf: ArrayBuffer = await packer.packing(pm, opt);
    packer.release();
    return buf;
  }

  /** 写入应用缓存目录（分享需要真实文件路径；相册对话框也需要源 uri） */
  static writeCache(ctx: common.UIAbilityContext, buf: ArrayBuffer): string {
    const path: string = ctx.cacheDir + '/report_' + ReportExporter.stamp() + '.png';
    const file = fileIo.openSync(path,
      fileIo.OpenMode.CREATE | fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.TRUNC);
    fileIo.writeSync(file.fd, buf);
    fileIo.closeSync(file);
    ReportExporter.pngPath = path;
    return path;
  }

  /**
   * 保存到系统相册。
   * 先申请权限；被拒时用「安全保存对话框」兜底；都不行才返回 needSetting=true。
   */
  static async saveToAlbum(ctx: common.UIAbilityContext, png: ArrayBuffer): Promise<AlbumSaveResult> {
    const pngPath: string = ReportExporter.writeCache(ctx, png);

    // 第一步：申请相册写入权限（模块里已在 module.json5 声明）
    let granted: boolean = false;
    try {
      const at: abilityAccessCtrl.AtManager = abilityAccessCtrl.createAtManager();
      const perm: Permissions = 'ohos.permission.WRITE_IMAGEVIDEO';
      const res = await at.requestPermissionsFromUser(ctx, [perm]);
      granted = res.authResults.length > 0 && res.authResults[0] === 0;
    } catch (err) {
      granted = false;
    }

    if (granted) {
      try {
        const helper: photoAccessHelper.PhotoAccessHelper = photoAccessHelper.getPhotoAccessHelper(ctx);
        // createAsset 直接在相册里建一张图，返回 uri（形如 file://media/Photo/...）
        const uri: string = await helper.createAsset(photoAccessHelper.PhotoType.IMAGE, 'png');
        const file = fileIo.openSync(uri, fileIo.OpenMode.READ_WRITE);
        fileIo.writeSync(file.fd, png);
        fileIo.closeSync(file);
        return { ok: true, message: '已保存到相册', needSetting: false };
      } catch (err) {
        // 权限给了但写入失败（部分机型/模拟器无相册服务），继续走兜底
      }
    }

    // 第二步：兜底——系统"保存到相册"确认框，用户点一下即可，无需应用持有权限
    try {
      const ok: boolean = await ReportExporter.saveByDialog(ctx, pngPath, png);
      if (ok) {
        return { ok: true, message: '已保存到相册', needSetting: false };
      }
    } catch (err) {
      // 继续落到“去设置”提示
    }

    return {
      ok: false,
      message: granted ? '保存失败：相册不可用' : '没有相册写入权限',
      needSetting: !granted
    };
  }

  /** 安全保存对话框：把应用沙箱里的文件交给系统去建相册条目 */
  private static async saveByDialog(ctx: common.UIAbilityContext, pngPath: string,
                                    png: ArrayBuffer): Promise<boolean> {
    const config: photoAccessHelper.PhotoCreationConfig = {
      title: '守夜灯报告_' + ReportExporter.stamp(),
      fileNameExtension: 'png',
      photoType: photoAccessHelper.PhotoType.IMAGE,
      subtype: photoAccessHelper.PhotoSubtype.DEFAULT
    };
    const srcUri: string = fileUri.getUriFromPath(pngPath);
    const helper: photoAccessHelper.PhotoAccessHelper = photoAccessHelper.getPhotoAccessHelper(ctx);
    const targetUris: string[] = await helper.showAssetsCreationDialog([srcUri], [config]);
    if (targetUris.length === 0) {
      return false;
    }
    // 对话框只建了空条目，内容仍要应用自己写入
    const dst = fileIo.openSync(targetUris[0], fileIo.OpenMode.READ_WRITE | fileIo.OpenMode.CREATE);
    fileIo.writeSync(dst.fd, png);
    fileIo.closeSync(dst);
    return true;
  }

  /** 调系统分享面板，把长图作为图片附件分享给家人（微信/邮件/蓝牙等由系统决定） */
  static async share(ctx: common.UIAbilityContext): Promise<ShareResult> {
    if (ReportExporter.pngPath.length === 0) {
      return { ok: false, message: '还没有生成报告图' };
    }
    try {
      const record: systemShare.SharedRecord = {
        utd: utd.UniformDataType.PNG,
        uri: fileUri.getUriFromPath(ReportExporter.pngPath)
      };
      const data: systemShare.SharedData = new systemShare.SharedData(record);
      const controller: systemShare.ShareController = new systemShare.ShareController(data);
      const options: systemShare.ShareControllerOptions = {
        previewMode: systemShare.SharePreviewMode.DETAIL,
        selectionMode: systemShare.SelectionMode.SINGLE
      };
      await controller.show(ctx, options);
      return { ok: true, message: '' };
    } catch (err) {
      return { ok: false, message: '分享面板打开失败' };
    }
  }

  /** 权限被拒后引导到系统设置（不同机型设置页入口不同，失败时调用方给文字指引） */
  static async openPermissionSetting(ctx: common.UIAbilityContext): Promise<boolean> {
    try {
      const want: Want = {
        bundleName: 'com.huawei.hmos.settings',
        abilityName: 'com.huawei.hmos.settings.MainAbility'
      };
      await ctx.startAbility(want);
      return true;
    } catch (err) {
      return false;
    }
  }

  /** 最近一次生成的 PNG 路径（调试/排查用） */
  static lastPath(): string {
    return ReportExporter.pngPath;
  }
}
