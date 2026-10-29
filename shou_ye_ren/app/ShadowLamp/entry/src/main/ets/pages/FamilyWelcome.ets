/**
 * 首次启动 / 未加入家庭组时的入口页：创建家庭组 或 用 6 位密码加入。
 *
 * 为什么需要这一页：本题是“一位老人 + 多个监护人”。数据本身在网关，
 * 谁有资格一起看，需要一个“组”的概念；没有组的时候直接进首页，
 * 用户不知道密码是什么、也不知道自己算哪台设备的主人。
 *
 * 适老化：字号走 Theme.fs()（关怀模式放大 1.32 倍），主按钮高 56vp，
 * 说明文字用 15 号、强调色只用橙/蓝两色，避免低对比。
 *
 * 诚实说明：口令与成员关系存在网关内存里，网关重启即清零；
 * 生产环境应替换为云端账号体系（手机号登录 + 家庭组落库 + 邀请短信）。
 */
import { BusinessError } from '@kit.BasicServicesKit';
import { common } from '@kit.AbilityKit';
import { Theme } from '../common/Theme';
import { ApiClient, FamilyResult, Settings } from '../service/ApiClient';
import { FamilyStore } from '../service/FamilyStore';

@Entry
@Component
struct FamilyWelcome {
  @StorageLink('largeFont') largeFont: boolean = false;
  /** 0=还没选（显示两个入口） 1=创建 2=加入 */
  @State mode: number = 0;
  @State familyName: string = '';
  @State myName: string = '';
  @State inviteCode: string = '';
  @State baseUrl: string = Settings.baseUrl;
  @State busy: boolean = false;
  @State tip: string = '';
  @State tipOk: boolean = false;

  aboutToAppear(): void {
    const ctx: common.UIAbilityContext =
      this.getUIContext().getHostContext() as common.UIAbilityContext;
    this.initAll(ctx);
  }

  private async initAll(ctx: common.UIAbilityContext): Promise<void> {
    await Settings.init(ctx);
    await FamilyStore.init(ctx);
    // 已加入过就直接进首页，不用每次重开都看到这一页
    if (FamilyStore.joined) {
      this.goHome();
      return;
    }
    this.baseUrl = Settings.baseUrl;
  }

  /** 网关地址改了要先存，再调用接口，否则请求还打在旧地址上 */
  private async saveBase(): Promise<void> {
    const text: string = this.baseUrl.trim();
    if (text.length > 0) {
      await Settings.save(text);
    }
  }

  private async doCreate(): Promise<void> {
    const name: string = this.familyName.trim().length > 0 ? this.familyName.trim() : '我的家庭';
    const owner: string = this.myName.trim().length > 0 ? this.myName.trim() : '户主本人';
    this.busy = true;
    this.tip = '';
    await this.saveBase();
    const result: FamilyResult = await ApiClient.createFamily(name, owner);
    this.busy = false;
    if (result.ok && result.info !== null) {
      await FamilyStore.save(result.info, owner);
      this.goHome();
    } else {
      this.showTip(result.code === 'NETWORK' ? result.message
        : ('创建失败：' + result.message), false);
    }
  }

  private async doJoin(): Promise<void> {
    const code: string = this.inviteCode.trim();
    const name: string = this.myName.trim().length > 0 ? this.myName.trim() : '家人';
    if (code.length !== 6) {
      this.showTip('请先输入 6 位数字密码', false);
      return;
    }
    this.busy = true;
    this.tip = '';
    await this.saveBase();
    const result: FamilyResult = await ApiClient.joinFamily(code, name);
    this.busy = false;
    if (result.ok && result.info !== null) {
      await FamilyStore.save(result.info, name);
      this.goHome();
    } else {
      this.showTip(result.message.length > 0 ? result.message : '加入失败，请重试', false);
    }
  }

  /** 跳首页：用 UIContext 的 Router，router.* 静态接口已废弃；失败只提示不崩 */
  private goHome(): void {
    try {
      this.getUIContext().getRouter().replaceUrl({ url: 'pages/Index' });
    } catch (err) {
      const e = err as BusinessError;
      this.showTip('页面跳转失败：' + e.message, false);
    }
  }

  private showTip(text: string, ok: boolean): void {
    this.tip = text;
    this.tipOk = ok;
  }

  // ------------------------------------------------------------------ UI
  @Builder
  head() {
    Column() {
      Text('守夜灯')
        .fontSize(Theme.fs(30, this.largeFont))
        .fontWeight(FontWeight.Bold)
        .fontColor(Color.White)
      Text('独居老人居家安全监测 · 家庭组')
        .fontSize(Theme.fs(15, this.largeFont))
        .fontColor('#94A3B8')
        .margin({ top: 6 })
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
    .padding({ left: 20, right: 20, top: 28, bottom: 26 })
    .backgroundColor(Theme.navy)
  }

  @Builder
  card() {
    Column() {
      if (this.mode === 0) {
        Text('一位老人，一家人一起看')
          .fontSize(Theme.fs(22, this.largeFont))
          .fontWeight(FontWeight.Bold)
          .fontColor(Theme.navy)
          .width('100%')
        Text('首次使用请先创建一个家庭组；如果子女或社区已经建好，用 6 位密码加入。')
          .fontSize(Theme.fs(15, this.largeFont))
          .fontColor(Theme.slate)
          .lineHeight(Theme.fs(23, this.largeFont))
          .margin({ top: 10 })
          .width('100%')
        this.bigButton('创建家庭组', Theme.blue, true, 1)
        this.bigButton('用密码加入', Theme.card, false, 2)
      } else if (this.mode === 1) {
        this.sectionTitle('创建家庭组')
        this.field('家庭组名称', '如：张家 · 3 楼', this.familyName, 20, false, 1)
        this.field('我的称呼', '如：大女儿', this.myName, 12, false, 2)
        this.bigButton('确认创建', Theme.blue, true, 3)
      } else {
        this.sectionTitle('加入家庭组')
        this.field('6 位加入密码', '000000', this.inviteCode, 6, true, 3)
        this.field('我的称呼', '如：社区网格员小李', this.myName, 12, false, 2)
        this.bigButton('加入', Theme.blue, true, 4)
      }

      this.baseRow()

      if (this.tip.length > 0) {
        Text(this.tip)
          .fontSize(Theme.fs(15, this.largeFont))
          .fontColor(this.tipOk ? Theme.green : Theme.red)
          .lineHeight(Theme.fs(22, this.largeFont))
          .width('100%')
          .margin({ top: 12 })
      }
    }
    .width('100%')
    .padding(18)
    .backgroundColor(Theme.card)
    .borderRadius(Theme.radius)
  }

  @Builder
  sectionTitle(text: string) {
    Text(text)
      .fontSize(Theme.fs(22, this.largeFont))
      .fontWeight(FontWeight.Bold)
      .fontColor(Theme.navy)
      .width('100%')
      .margin({ bottom: 8 })
  }

  /**
   * 输入行：标题 + 输入框 + 提示。
   * which 决定写回哪个状态（1=家庭组名称 2=我的称呼 3=密码），
   * 不用 @Builder 传回调——ArkTS 的 @Builder 参数不是双向绑定，传引用会丢更新。
   */
  @Builder
  field(title: string, hint: string, value: string, maxLen: number,
        numeric: boolean, which: number) {
    Column() {
      Text(title)
        .fontSize(Theme.fs(15, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
      TextInput({ text: value, placeholder: hint })
        .type(numeric ? InputType.Number : InputType.Normal)
        .maxLength(maxLen)
        .fontSize(Theme.fs(20, this.largeFont))
        .fontColor(Theme.navy)
        .placeholderColor('#94A3B8')
        .height(Theme.fs(52, this.largeFont))
        .backgroundColor('#F2F4F8')
        .borderRadius(10)
        .margin({ top: 6 })
        .onChange((v: string) => {
          if (which === 1) {
            this.familyName = v;
          } else if (which === 3) {
            this.inviteCode = v;
          } else {
            this.myName = v;
          }
        })
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
    .margin({ bottom: 12 })
  }

  @Builder
  baseRow() {
    Column() {
      Text('网关地址')
        .fontSize(Theme.fs(14, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
      TextInput({ text: this.baseUrl, placeholder: 'http://电脑局域网IP:8780' })
        .fontSize(Theme.fs(16, this.largeFont))
        .fontColor(Theme.navy)
        .height(Theme.fs(48, this.largeFont))
        .backgroundColor('#F2F4F8')
        .borderRadius(10)
        .margin({ top: 6 })
        .onChange((v: string) => {
          this.baseUrl = v;
        })
      Text('网关启动时会打印这个地址；手机和电脑要在同一个 WiFi 下。')
        .fontSize(Theme.fs(13, this.largeFont))
        .fontColor(Theme.muted)
        .lineHeight(Theme.fs(19, this.largeFont))
        .width('100%')
        .margin({ top: 6 })
    }
    .width('100%')
    .alignItems(HorizontalAlign.Start)
    .margin({ top: 16 })
  }

  /** 主按钮：关怀模式下也跟着放大，56 高度保证好点 */
  @Builder
  bigButton(label: string, bg: string, filled: boolean, action: number) {
    Row() {
      if (this.busy) {
        Text('处理中…')
          .fontSize(Theme.fs(19, this.largeFont))
          .fontColor(filled ? Color.White : Theme.navy)
      } else {
        Text(label)
          .fontSize(Theme.fs(19, this.largeFont))
          .fontWeight(FontWeight.Medium)
          .fontColor(filled ? Color.White : Theme.navy)
      }
    }
    .width('100%')
    .height(56)
    .justifyContent(FlexAlign.Center)
    .backgroundColor(filled ? bg : Theme.card)
    .borderRadius(12)
    .border({ width: filled ? 0 : 1, color: Theme.line })
    .margin({ top: 12 })
    .onClick(() => {
      if (this.busy) {
        return;
      }
      if (action === 1) {
        this.mode = 1;
      } else if (action === 2) {
        this.mode = 2;
      } else if (action === 3) {
        this.doCreate();
      } else {
        this.doJoin();
      }
    })
  }

  build() {
    Column() {
      this.head()
      Scroll() {
        Column() {
          this.card()
          if (this.mode !== 0) {
            Text('返回')
              .fontSize(Theme.fs(16, this.largeFont))
              .fontColor(Theme.blue)
              .width('100%')
              .textAlign(TextAlign.Center)
              .padding({ top: 18, bottom: 8 })
              .onClick(() => {
                this.mode = 0;
                this.tip = '';
              })
          }
          Text('家庭组成员关系保存在网关内存中，重启网关会重置。')
            .fontSize(Theme.fs(13, this.largeFont))
            .fontColor(Theme.muted)
            .width('100%')
            .textAlign(TextAlign.Center)
            .margin({ top: 18 })
        }
        .width('100%')
        .padding({ left: 16, right: 16, bottom: 24 })
      }
      .layoutWeight(1)
      .align(Alignment.Top)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Theme.bg)
  }
}
