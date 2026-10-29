/**
 * 应用入口页：底部四个页签 + 全局 3 秒心跳。
 *
 * 心跳的设计取舍：不做推送（要服务端与推送证书），改用 3 秒轮询 + 各页 @Watch 刷新。
 * 局域网演示够用，且断电重启后自然恢复。
 */
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { Theme } from '../common/Theme';
import { ApiClient, Settings } from '../service/ApiClient';
import { AlarmEvent } from '../model/Models';
import { Alerter } from '../service/Alerter';
import { Guide } from '../view/Guide';
import { AlarmStore } from '../service/AlarmStore';
import { Outbox } from '../service/Outbox';
import { FamilyStore } from '../service/FamilyStore';
import { HomeTab } from '../view/HomeTab';
import { AlarmTab } from '../view/AlarmTab';
import { ReportTab } from '../view/ReportTab';
import { MineTab } from '../view/MineTab';

@Entry
@Component
struct Index {
  /** 关怀模式（大字）开关，由「我的」页写入 AppStorage */
  @StorageLink('reportPreviewOpen') reportPreviewOpen: boolean = false;
  @StorageLink('largeFont') largeFont: boolean = false;

  @State current: number = 0;
  @State tick: number = 0;
  /** 首次启动显示新手引导 */
  @State showGuide: boolean = false;
  private timer: number = -1;
  /** 上一轮看到的待确认告警：等级序与条数，用来判断“有没有新情况” */
  private lastLevelRank: number = 0;
  private lastPendingCount: number = 0;
  /** 待补发队列的补发节奏（9 秒一次，比心跳慢，避免和轮询打架） */
  private flushTimer: number = -1;

  aboutToAppear(): void {
    const ctx: common.UIAbilityContext =
      this.getUIContext().getHostContext() as common.UIAbilityContext;
    this.bootstrap(ctx);
    this.showGuide = !Settings.guideDone;
    this.timer = setInterval(() => {
      this.tick += 1;
      this.watchAlarms();
    }, 3000);
    this.flushTimer = setInterval(() => {
      Outbox.flush();
    }, 9000);
  }

  /** 先完成设置与本地库初始化，再进入轮询 */
  private async bootstrap(ctx: common.UIAbilityContext): Promise<void> {
    await Settings.init(ctx);
    await FamilyStore.init(ctx);
    // 没加入家庭组 → 先去“创建/加入”页；加入后再回到首页
    if (!FamilyStore.joined) {
      try {
        this.getUIContext().getRouter().replaceUrl({ url: 'pages/FamilyWelcome' });
      } catch (err) {
        const e = err as BusinessError;
        console.error('[family] 跳转失败：' + e.message);
      }
      return;
    }
    await AlarmStore.init(ctx);
    await Alerter.init(ctx);
    await Outbox.init(ctx);
    this.tick += 1;
  }

  /**
   * 前台告警提醒：3 秒轮询一次待确认列表，
   * 只有“出现更高等级”或“数量变多”才震动/响提示音，避免把提示音刷成噪音。
   * 说明：不是推送，APP 退到后台后不会响——这一点在答辩时会如实讲。
   */
  private async watchAlarms(): Promise<void> {
    const list: AlarmEvent[] | null = await ApiClient.alarms('pending');
    if (list === null) {
      return;
    }
    let rank: number = 0;
    for (const item of list) {
      const value: number = item.level === 'L3' ? 3 : (item.level === 'L2' ? 2 : 1);
      if (value > rank) {
        rank = value;
      }
    }
    const count: number = list.length;
    const worse: boolean = rank > this.lastLevelRank || count > this.lastPendingCount;
    this.lastLevelRank = rank;
    this.lastPendingCount = count;
    if (worse && rank > 0) {
      Alerter.notify(rank === 3 ? 'L3' : (rank === 2 ? 'L2' : 'L1'));
    }
  }

  aboutToDisappear(): void {
    if (this.timer !== -1) {
      clearInterval(this.timer);
    }
    if (this.flushTimer !== -1) {
      clearInterval(this.flushTimer);
    }
  }

  @Builder
  tabBar(index: number, title: string, icon: string) {
    Column() {
      Text(icon)
        // 图标不按 1.2 倍算：老人对底部导航的图标更大更敏感，关怀模式直接给 26
        .fontSize(this.largeFont ? 26 : 18)
        .fontColor(this.current === index ? Theme.navy : Theme.muted)
      Text(title)
        .fontSize(Theme.fs(11, this.largeFont))
        .fontColor(this.current === index ? Theme.navy : Theme.muted)
        .margin({ top: 2 })
    }
    .width('100%')
    .height('100%')
    .justifyContent(FlexAlign.Center)
  }

  build() {
    if (this.showGuide) {
      Guide({
        onDone: () => {
          this.showGuide = false;
          Settings.markGuideDone();
        }
      })
    } else {
      this.mainPage()
    }
  }

  @Builder
  mainPage() {
    Column() {
      Tabs({ barPosition: BarPosition.End, index: this.current }) {
        TabContent() {
          HomeTab({ tick: this.tick, onJump: (index: number) => { this.current = index; } })
        }.tabBar(this.tabBar(0, '首页', '◉'))

        TabContent() {
          AlarmTab({ tick: this.tick })
        }.tabBar(this.tabBar(1, '告警', '△'))

        TabContent() {
          ReportTab({ tick: this.tick })
        }.tabBar(this.tabBar(2, '报告', '▤'))

        TabContent() {
  MineTab({ tick: this.tick, active: this.current === 3 })
        }.tabBar(this.tabBar(3, '我的', '◍'))
      }
      .barHeight(this.reportPreviewOpen ? 0 : 58)
      .barMode(BarMode.Fixed)
      .backgroundColor(Theme.card)
      .onChange((index: number) => {
        this.current = index;
      })
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Theme.bg)
  }
}
