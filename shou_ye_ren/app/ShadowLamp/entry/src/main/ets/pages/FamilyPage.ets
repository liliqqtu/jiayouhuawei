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

  aboutToAppear(): void {
    // 先用本地副本渲染，避免网络慢时页面空白
    this.familyName = FamilyStore.familyName;
    this.inviteCode = FamilyStore.inviteCode;
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

            Text(m.isOwner ? '户主' : '监护人')
              .fontSize(Theme.fs(13, this.largeFont))
              .fontColor(m.isOwner ? Theme.orange : Theme.muted)
              .padding({ left: 8, right: 8, top: 3, bottom: 3 })
              .backgroundColor(m.isOwner ? '#FFF3E8' : '#F2F4F8')
              .borderRadius(6)
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
