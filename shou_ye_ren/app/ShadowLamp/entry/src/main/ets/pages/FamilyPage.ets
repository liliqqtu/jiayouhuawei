/**
 * 家庭组页面
 */
import { promptAction } from '@kit.ArkUI';
import { BusinessError } from '@kit.BasicServicesKit';
import { common } from '@kit.AbilityKit';
import { pasteboard } from '@kit.BasicServicesKit';
import { Theme } from '../common/Theme';
import { ApiClient, FamilyResult, Settings } from '../service/ApiClient';
import { FamilyStore } from '../service/FamilyStore';
import { FamilyInfo, FamilyMember, SensorSnapshot } from '../model/Models';

@Entry
@Component
struct FamilyPage {
  @StorageLink('largeFont') largeFont: boolean = false;
  @State family: FamilyInfo | null = null;
  @State members: FamilyMember[] = [];
  @State inviteCode: string = '';
  @State familyName: string = '';
  @State showInvite: boolean = false;
  @State tip: string = '';
  @State dataSource: string = '未知';
  @State sourceIsTest: boolean = true;
  @State confirmLeave: boolean = false;
  /** 我是不是管理员（户主默认是管理员）：决定要不要显示增删成员的入口 */
  @State isAdmin: boolean = false;
  /** 添加成员面板是否展开 */
  @State showAdd: boolean = false;
  @State newMemberName: string = '';
  /** 新成员是否直接设为管理员 */
  @State newMemberAsAdmin: boolean = false;
  @State adding: boolean = false;
  /** 点了“移除”还没确认的那一行：再点一次才真删，防误触 */
  @State pendingRemoveId: string = '';

  aboutToAppear(): void {
    // 先用本地副本渲染，避免网络慢时页面空白
    this.familyName = FamilyStore.familyName;
    this.inviteCode = FamilyStore.inviteCode;
    this.isAdmin = FamilyStore.isAdmin();
    this.refresh();
    this.loadSource();
  }

  /** 从网关刷新成员表；网关重启过（内存丢了）会返回 FAMILY_NOT_FOUND */
  private async refresh(): Promise<void> {
    if (FamilyStore.familyId.length === 0) {
      this.tip = '还没有加入家庭组';
      return;
    }
    const result: FamilyResult = await ApiClient.familyInfo(FamilyStore.familyId);
    if (result.ok && result.info !== null) {
      this.family = result.info;
      this.members = result.info.members;
      this.inviteCode = result.info.inviteCode;
      this.familyName = result.info.familyName;
      this.syncMyRole(result.info);
      this.tip = '';
    } else if (result.code === 'FAMILY_NOT_FOUND') {
      this.tip = '网关重启过，这个家庭组已重置，请重新创建或加入';
      this.members = [];
    } else {
      this.tip = '连不上网关，以下为本地保存的信息';
    }
  }

  /** 读最新一张快照的来源标注：真实设备 / 模拟演示 / 未知 */
  private async loadSource(): Promise<void> {
    const snap: SensorSnapshot | null = await ApiClient.snapshot();
    if (snap === null) {
      this.dataSource = '未知（网关未连接）';
      this.sourceIsTest = true;
      return;
    }
    this.sourceIsTest = snap.source !== 'hardware';
    this.dataSource = snap.source === 'hardware' ? '真实设备' : '模拟演示';
  }

  private inviteText(): string {
    return 'shadowlamp://join?gw=' + Settings.baseUrl + '&familyId=' + FamilyStore.familyId
      + '&code=' + this.inviteCode;
  }

  /** 复制密码：粘贴板失败就退回提示“请手动念给对方” */
  private async copyCode(): Promise<void> {
    try {
      const data = pasteboard.createData(pasteboard.MIMETYPE_TEXT_PLAIN, this.inviteCode);
      await pasteboard.getSystemPasteboard().setData(data);
      promptAction.showToast({ message: '已复制 6 位密码' });
    } catch (err) {
      promptAction.showToast({ message: '复制失败，请手动念给对方' });
    }
  }

  private async leave(): Promise<void> {
    await FamilyStore.clear();
    try {
      this.getUIContext().getRouter().replaceUrl({ url: 'pages/FamilyWelcome' });
    } catch (err) {
      const e = err as BusinessError;
      promptAction.showToast({ message: '跳转失败：' + e.message });
    }
  }

  /**
   * 我的身份以网关成员表为准（本地 role 可能是加入时记的，管理员被降级后要跟着变）。
   * 名字对不上就沿用本地判断，避免网关数据异常时整页失去管理入口。
   */
  private syncMyRole(info: FamilyInfo | null): void {
    if (info === null) {
      this.isAdmin = FamilyStore.isAdmin();
      return;
    }
    let matched: boolean = false;
    for (let i = 0; i < info.members.length; i++) {
      if (info.members[i].name === FamilyStore.memberName) {
        this.isAdmin = info.members[i].isAdmin;
        // 网关可能刚把我提成管理员（或被降级）：本地身份跟着对齐
        FamilyStore.applyRole(info.members[i].role, info.members[i].memberId);
        matched = true;
        break;
      }
    }
    if (!matched) {
      this.isAdmin = FamilyStore.isAdmin();
    }
  }

  /** 管理员直接添加成员：填称呼、可选设为管理员 */
  private async addMember(): Promise<void> {
    if (this.adding) {
      return;
    }
    const name: string = this.newMemberName.trim();
    if (name.length === 0) {
      promptAction.showToast({ message: '先填写成员称呼' });
      return;
    }
    if (name.length > 12) {
      promptAction.showToast({ message: '成员称呼最长 12 个字' });
      return;
    }
    this.adding = true;
    const role: string = this.newMemberAsAdmin ? 'admin' : 'guardian';
    const result: FamilyResult =
      await ApiClient.familyAddMember(FamilyStore.familyId, FamilyStore.memberName, name, role);
    this.adding = false;
    if (result.ok && result.info !== null) {
      this.family = result.info;
      this.members = result.info.members;
      this.newMemberName = '';
      this.newMemberAsAdmin = false;
      this.showAdd = false;
      this.syncMyRole(result.info);
      promptAction.showToast({ message: '已添加 ' + name });
    } else {
      const msg: string = result.message.length > 0 ? result.message : '添加失败，请检查网关连接';
      promptAction.showToast({ message: msg });
    }
  }

  /** 移除成员：第一次点只记下来，第二次点才真删（误触代价太大） */
  private async removeMember(member: FamilyMember): Promise<void> {
    if (this.pendingRemoveId !== member.memberId) {
      this.pendingRemoveId = member.memberId;
      promptAction.showToast({ message: '再点一次确认移除 ' + member.name });
      return;
    }
    this.pendingRemoveId = '';
    const result: FamilyResult = await ApiClient.familyRemoveMember(
      FamilyStore.familyId, FamilyStore.memberName, member.memberId);
    if (result.ok && result.info !== null) {
      this.family = result.info;
      this.members = result.info.members;
      this.syncMyRole(result.info);
      promptAction.showToast({ message: '已移除 ' + member.name });
    } else {
      const msg: string = result.message.length > 0 ? result.message : '移除失败，请检查网关连接';
      promptAction.showToast({ message: msg });
    }
  }

  // ------------------------------------------------------------------ UI
  @Builder
  header() {
    Row() {
      Text('返回')
        .fontSize(Theme.fs(16, this.largeFont))
        .fontColor('#94A3B8')
        .onClick(() => {
          try {
            this.getUIContext().getRouter().back();
          } catch (err) {
            const e = err as BusinessError;
            promptAction.showToast({ message: '返回失败：' + e.message });
          }
        })
      Text('家庭组')
        .fontSize(Theme.fs(20, this.largeFont))
        .fontWeight(FontWeight.Bold)
        .fontColor(Color.White)
        .margin({ left: 14 })
      Blank()
      Text('邀请家人')
        .fontSize(Theme.fs(15, this.largeFont))
        .fontColor(Theme.blueLight)
        .onClick(() => {
          this.showInvite = true;
        })
    }
    .width('100%')
    .padding({ left: 16, right: 16, top: 18, bottom: 18 })
    .backgroundColor(Theme.navy)
  }

  @Builder
  infoCard() {
    Column() {
      Row() {
        Text(this.familyName.length > 0 ? this.familyName : '未命名家庭组')
          .fontSize(Theme.fs(24, this.largeFont))
          .fontWeight(FontWeight.Bold)
          .fontColor(Theme.navy)
          .layoutWeight(1)
        Text(FamilyStore.roleText())
          .fontSize(Theme.fs(14, this.largeFont))
          .fontColor(Theme.blue)
          .padding({ left: 10, right: 10, top: 4, bottom: 4 })
          .backgroundColor('#EFF4FF')
          .borderRadius(8)
      }
      .width('100%')

      Text('我：' + (FamilyStore.memberName.length > 0 ? FamilyStore.memberName : '未命名')
        + ' · 家庭组编号 ' + (FamilyStore.familyId.length > 0 ? FamilyStore.familyId : '—')
        + ' · 共 ' + this.members.length + ' 位成员')
        .fontSize(Theme.fs(14, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
        .margin({ top: 10 })
        .lineHeight(Theme.fs(21, this.largeFont))
    }
    .width('100%')
    .padding(16)
    .backgroundColor(Theme.card)
    .borderRadius(Theme.radius)
  }

  @Builder
  memberCard() {
    Column() {
      Text('成员')
        .fontSize(Theme.fs(16, this.largeFont))
        .fontWeight(FontWeight.Medium)
        .fontColor(Theme.navy)
        .width('100%')
      if (this.members.length === 0) {
        Text(this.tip.length > 0 ? this.tip : '还没有读到成员列表')
          .fontSize(Theme.fs(14, this.largeFont))
          .fontColor(Theme.orange)
          .width('100%')
          .margin({ top: 10 })
          .lineHeight(Theme.fs(21, this.largeFont))
      } else {
        ForEach(this.members, (m: FamilyMember) => {
          Row() {
            Column() {
              Text(m.name)
                .fontSize(Theme.fs(17, this.largeFont))
                .fontColor(Theme.navy)
              Text(m.roleText + ' · 加入于 ' + this.shortTime(m.joinedAt))
                .fontSize(Theme.fs(13, this.largeFont))
                .fontColor(Theme.muted)
                .margin({ top: 4 })
            }
            .alignItems(HorizontalAlign.Start)
            .layoutWeight(1)

            Text(m.roleText)
              .fontSize(Theme.fs(13, this.largeFont))
              .fontColor(m.isAdmin ? Theme.orange : Theme.muted)
              .padding({ left: 8, right: 8, top: 3, bottom: 3 })
              .backgroundColor(m.isAdmin ? '#FFF3E8' : '#F2F4F8')
              .borderRadius(6)

            // 户主不能被移除；管理员能移除其他成员（含其他管理员）
            if (this.isAdmin && !m.isOwner) {
              Text(this.pendingRemoveId === m.memberId ? '确认移除' : '移除')
                .fontSize(Theme.fs(13, this.largeFont))
                .fontColor(this.pendingRemoveId === m.memberId ? Color.White : Theme.red)
                .padding({ left: 10, right: 10, top: 5, bottom: 5 })
                .backgroundColor(this.pendingRemoveId === m.memberId ? Theme.red : '#FEE2E2')
                .borderRadius(6)
                .margin({ left: 8 })
                .onClick(() => {
                  this.removeMember(m);
                })
            }
          }
          .width('100%')
          .padding({ top: 12, bottom: 12 })
          .border({ width: { top: 1 }, color: Theme.line })
        }, (m: FamilyMember) => m.memberId)
      }
    }
    .width('100%')
    .padding(16)
    .backgroundColor(Theme.card)
    .borderRadius(Theme.radius)
    .margin({ top: 12 })
  }

  /** 管理员面板：户主/管理员可以加人，也可以把家人提成管理员一起管 */
  @Builder
  adminCard() {
    Column() {
      Row() {
        Text('成员管理')
          .fontSize(Theme.fs(16, this.largeFont))
          .fontWeight(FontWeight.Medium)
          .fontColor(Theme.navy)
          .layoutWeight(1)
        Text(this.showAdd ? '收起' : '添加成员')
          .fontSize(Theme.fs(14, this.largeFont))
          .fontColor(Theme.blue)
          .padding({ left: 10, right: 10, top: 6, bottom: 6 })
          .backgroundColor('#EFF4FF')
          .borderRadius(8)
          .onClick(() => {
            this.showAdd = !this.showAdd;
          })
      }
      .width('100%')

      Text('你是' + FamilyStore.roleText() + '，可以直接添加家人，或移除不用的成员。')
        .fontSize(Theme.fs(13, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
        .margin({ top: 8 })
        .lineHeight(Theme.fs(19, this.largeFont))

      if (this.showAdd) {
        TextInput({ text: this.newMemberName, placeholder: '成员称呼，例如 小儿子' })
          .width('100%')
          .height(44)
          .fontSize(Theme.fs(15, this.largeFont))
          .backgroundColor('#F2F4F8')
          .borderRadius(10)
          .margin({ top: 12 })
          .onChange((value: string) => {
            this.newMemberName = value;
          })

        Row() {
          Text('把这个成员设为管理员')
            .fontSize(Theme.fs(14, this.largeFont))
            .fontColor(Theme.slate)
            .layoutWeight(1)
          Toggle({ type: ToggleType.Switch, isOn: this.newMemberAsAdmin })
            .onChange((isOn: boolean) => {
              this.newMemberAsAdmin = isOn;
            })
        }
        .width('100%')
        .height(40)
        .margin({ top: 6 })

        Text(this.adding ? '添加中……' : '确认添加')
          .fontSize(Theme.fs(16, this.largeFont))
          .fontColor(Color.White)
          .width('100%')
          .height(46)
          .textAlign(TextAlign.Center)
          .backgroundColor(this.adding ? Theme.muted : Theme.blue)
          .borderRadius(10)
          .margin({ top: 8 })
          .onClick(() => {
            this.addMember();
          })

        Text('管理员能添加、移除成员；监护人只能查看数据。')
          .fontSize(Theme.fs(12, this.largeFont))
          .fontColor(Theme.muted)
          .width('100%')
          .margin({ top: 8 })
          .lineHeight(Theme.fs(18, this.largeFont))
      }
    }
    .width('100%')
    .padding(16)
    .backgroundColor(Theme.card)
    .borderRadius(Theme.radius)
    .margin({ top: 12 })
  }

  @Builder
  codeCard() {
    Column() {
      Text('加入密码（告诉要一起看的家人）')
        .fontSize(Theme.fs(14, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
      Row() {
        Text(this.inviteCode.length > 0 ? this.inviteCode : '— — — — — —')
          .fontSize(Theme.fs(34, this.largeFont))
          .fontWeight(FontWeight.Bold)
          .fontColor(Theme.navy)
          .letterSpacing(6)
          .layoutWeight(1)
        Text('复制')
          .fontSize(Theme.fs(15, this.largeFont))
          .fontColor(Theme.blue)
          .padding({ left: 14, right: 14, top: 8, bottom: 8 })
          .backgroundColor('#EFF4FF')
          .borderRadius(8)
          .onClick(() => {
            this.copyCode();
          })
      }
      .width('100%')
      .alignItems(VerticalAlign.Center)
      .margin({ top: 8 })
    }
    .width('100%')
    .padding(16)
    .backgroundColor(Theme.card)
    .borderRadius(Theme.radius)
    .margin({ top: 12 })
  }

  /** 邀请浮层：大号密码 + 复制 + 二维码（网关地址 + 家庭组 ID + 密码） */
  @Builder
  inviteSheet() {
    Column() {
      Text('邀请家人')
        .fontSize(Theme.fs(20, this.largeFont))
        .fontWeight(FontWeight.Bold)
        .fontColor(Theme.navy)
        .width('100%')
      Text('方式一：把 6 位密码念给对方，在“加入家庭组”页输入')
        .fontSize(Theme.fs(14, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
        .margin({ top: 8 })
        .lineHeight(Theme.fs(21, this.largeFont))

      Row() {
        Text(this.inviteCode)
          .fontSize(Theme.fs(36, this.largeFont))
          .fontWeight(FontWeight.Bold)
          .fontColor(Theme.navy)
          .letterSpacing(8)
          .layoutWeight(1)
        Text('复制')
          .fontSize(Theme.fs(16, this.largeFont))
          .fontColor(Color.White)
          .padding({ left: 16, right: 16, top: 10, bottom: 10 })
          .backgroundColor(Theme.blue)
          .borderRadius(10)
          .onClick(() => {
            this.copyCode();
          })
      }
      .width('100%')
      .alignItems(VerticalAlign.Center)
      .padding({ top: 14, bottom: 14 })

      Text('方式二：让对方扫这个二维码（含网关地址与家庭组编号）')
        .fontSize(Theme.fs(14, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
        .margin({ bottom: 10 })
        .lineHeight(Theme.fs(21, this.largeFont))

      Column() {
        QRCode(this.inviteText())
          .width(180)
          .height(180)
          .color(Theme.navy)
      }
      .padding(12)
      .backgroundColor(Color.White)
      .borderRadius(12)
      .border({ width: 1, color: Theme.line })

      Text('注：二维码本期用于展示与转发，扫码自动解析未接入，加入仍用手输密码。')
        .fontSize(Theme.fs(12, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
        .margin({ top: 10 })
        .lineHeight(Theme.fs(18, this.largeFont))

      Text('关闭')
        .fontSize(Theme.fs(17, this.largeFont))
        .fontColor(Theme.blue)
        .width('100%')
        .height(50)
        .textAlign(TextAlign.Center)
        .margin({ top: 10 })
        .onClick(() => {
          this.showInvite = false;
        })
    }
    .width('100%')
    .padding(20)
    .backgroundColor(Theme.card)
    .borderRadius({ topLeft: 20, topRight: 20 })
  }

  @Builder
  footer() {
    Column() {
      Text('数据来源：' + this.dataSource + (this.sourceIsTest ? '（非真实传感器采集）' : ''))
        .fontSize(Theme.fs(13, this.largeFont))
        .fontColor(this.sourceIsTest ? Theme.orange : Theme.muted)
        .width('100%')
      Text('网关：' + Settings.baseUrl)
        .fontSize(Theme.fs(13, this.largeFont))
        .fontColor(Theme.muted)
        .width('100%')
        .margin({ top: 6 })
      if (this.tip.length > 0) {
        Text(this.tip)
          .fontSize(Theme.fs(13, this.largeFont))
          .fontColor(Theme.orange)
          .width('100%')
          .margin({ top: 8 })
          .lineHeight(Theme.fs(19, this.largeFont))
      }
      if (this.confirmLeave) {
        Text('再点一次确认退出家庭组')
          .fontSize(Theme.fs(14, this.largeFont))
          .fontColor(Theme.red)
          .width('100%')
          .margin({ top: 12 })
          .onClick(() => {
            this.leave();
          })
      } else {
        Text('退出家庭组')
          .fontSize(Theme.fs(14, this.largeFont))
          .fontColor(Theme.muted)
          .width('100%')
          .margin({ top: 12 })
          .onClick(() => {
            this.confirmLeave = true;
          })
      }
    }
    .width('100%')
    .padding({ left: 4, right: 4, top: 16 })
  }

  /** 2026-10-27T20:15:00+08:00 → 10-27 20:15，给成员列表省地方 */
  private shortTime(iso: string): string {
    if (iso.length >= 16) {
      return iso.substring(5, 10) + ' ' + iso.substring(11, 16);
    }
    return iso;
  }

  build() {
    Stack() {
      Column() {
        this.header()
        Scroll() {
          Column() {
            this.infoCard()
            this.memberCard()
            if (this.isAdmin) {
              this.adminCard()
            }
            this.codeCard()
            this.footer()
          }
          .width('100%')
          .padding({ left: 16, right: 16, bottom: 28 })
        }
        .layoutWeight(1)
        .align(Alignment.Top)
      }
      .width('100%')
      .height('100%')
      .backgroundColor(Theme.bg)

      if (this.showInvite) {
        // 用 Stack + 半透明遮罩代替 bindSheet：少一个 API 依赖，行为更好预测
        Column() {
          Blank().layoutWeight(1)
            .width('100%')
            .onClick(() => {
              this.showInvite = false;
            })
          this.inviteSheet()
        }
        .width('100%')
        .height('100%')
        .backgroundColor('#66000000')
      }
    }
    .width('100%')
    .height('100%')
  }
}
