/**
 * 报告：与自己过去一周比，而不是与别人比。
 * 只展示网关算好的结论（频次、环比、基线），不在端上重算，保证与文档口径一致。
 */
import { common } from '@kit.AbilityKit';
import { image } from '@kit.ImageKit';
import { promptAction } from '@kit.ArkUI';
import { Theme } from '../common/Theme';
import { ApiClient, Settings } from '../service/ApiClient';
import { AlbumSaveResult, ReportExporter, ShareResult } from '../service/ReportExporter';
import { DailyReport, MonthlyReport, SensorSnapshot, WeeklyReport } from '../model/Models';

@Component
export struct ReportTab {
  /** 关怀模式（大字）开关，由「我的」页写入 AppStorage */
  @StorageLink('largeFont') largeFont: boolean = false;

  @Prop @Watch('onTick') tick: number = 0;
  @State daily: DailyReport | null = null;
  @State weekly: WeeklyReport | null = null;
  @State monthly: MonthlyReport | null = null;
  @State monthOffset: number = 0;       /* 0=本月，1=上月；只做两个入口，不做日历选择 */
  @State monthKey: string = '';         /* 正在查看的月份，形如 2026-10 */
  @State temps: number[] = [];          /* 最近若干帧的温度，用于环境趋势 */
  @State loaded: boolean = false;
  @State exporting: boolean = false;
  @State exportTip: string = '';
  /* 导出预览（长图 Modal） */
  @State showPreview: boolean = false;
  @State previewing: boolean = false;      /* 正在渲染长图 */
  @State previewPm: image.PixelMap | null = null;
  @State zoom: number = 1;                 /* 双指缩放后倍率 */
  @State baseZoom: number = 1;             /* 上次手势结束时的倍率，用于增量 */
  @State panX: number = 0;
  @State panY: number = 0;
  @State saving: boolean = false;
  @State saveTip: string = '';

  aboutToAppear(): void {
    this.load();
  }

  onTick(): void {
    this.load();
  }

  /** yyyyMMdd_HHmmss，文件名带时间，多次导出不会互相覆盖 */
  private stamp(): string {
    const d: Date = new Date();
    const p2 = (n: number): string => (n < 10 ? '0' + n.toString() : n.toString());
    return d.getFullYear().toString() + p2(d.getMonth() + 1) + p2(d.getDate()) +
      '_' + p2(d.getHours()) + p2(d.getMinutes()) + p2(d.getSeconds());
  }

  /**
   * 导出当前报告页为 PNG（评审材料 / 发给家属 / 贴到文档里都用得上）。
   *
   * 走系统能力：componentSnapshot 当前页面 -> ImagePacker 编码 PNG ->
   * 写到应用沙箱 filesDir。不需要任何权限，也不上传网络（老人数据不出家门）。
   * 取文件：hdc file recv /data/app/el2/100/base/<bundle>/haps/entry/files/report_*.png
   */
  /**
   * 导出入口：先离屏渲染长图（标题 + 图表 + 落款），成功后弹全屏预览。
   * 不再静默写沙箱——用户看得见、能保存、能分享。
   */
  async openPreview(): Promise<void> {
    if (this.previewing) {
      return;
    }
    this.previewing = true;
    this.saveTip = '';
    this.zoom = 1;
    this.baseZoom = 1;
    this.panX = 0;
    this.panY = 0;
    this.previewPm = null;
    this.showPreview = true;
    AppStorage.setOrCreate('reportPreviewOpen', true);
    try {
      // 离屏渲染：不受滚动视口裁剪，长图一次画完
      const pm: image.PixelMap = await this.getUIContext().getComponentSnapshot().createFromBuilder(() => {
        this.longReport();
      }, 260);
      this.previewPm = pm;
      this.previewing = false;
      return;
    } catch (err) {
      // 兜底：部分设备不支持离屏渲染，退化为当前页面截图
      try {
        const pm2: image.PixelMap = await this.getUIContext().getComponentSnapshot().get('reportSnapshot');
        this.previewPm = pm2;
        this.saveTip = '长图渲染不可用，已改用当前页面截图';
      } catch (err2) {
        this.saveTip = '生成图片失败，请重试';
      }
    }
    this.previewing = false;
  }

  /** 保存到相册：申请权限 -> PhotoAccessHelper 写相册（失败走安全对话框） */
  private async doSave(): Promise<void> {
    if (this.saving || this.previewPm === null) {
      if (this.previewPm === null) {
        this.saveTip = '图片还在生成，请稍候再点';
      }
      return;
    }
    this.saving = true;
    this.saveTip = '正在保存…';
    const ctx: common.UIAbilityContext =
      this.getUIContext().getHostContext() as common.UIAbilityContext;
    try {
      const png: ArrayBuffer = await ReportExporter.toPng(this.previewPm);
      const res: AlbumSaveResult = await ReportExporter.saveToAlbum(ctx, png);
      if (res.ok) {
        this.showPreview = false;
        AppStorage.setOrCreate('reportPreviewOpen', false);
        promptAction.showToast({ message: '已保存到相册', duration: 2000 });
      } else if (res.needSetting) {
        this.saveTip = res.message;
        this.askPermissionSetting();
      } else {
        this.saveTip = res.message;
      }
    } catch (err) {
      this.saveTip = '保存失败，请重试';
    }
    this.saving = false;
  }

  /** 分享给家人：系统 ShareKit 面板，图片作为附件 */
  private async doShare(): Promise<void> {
    if (this.previewPm === null) {
      this.saveTip = '图片还在生成，请稍候再点';
      return;
    }
    const ctx: common.UIAbilityContext =
      this.getUIContext().getHostContext() as common.UIAbilityContext;
    try {
      const png: ArrayBuffer = await ReportExporter.toPng(this.previewPm);
      ReportExporter.writeCache(ctx, png);
      const res: ShareResult = await ReportExporter.share(ctx);
      if (res.ok) {
        // 系统分享面板已拉起，收起 Modal，避免遮挡
        this.showPreview = false;
        AppStorage.setOrCreate('reportPreviewOpen', false);
      } else {
        this.saveTip = res.message;
      }
    } catch (err) {
      this.saveTip = '分享失败，请重试';
    }
  }

  /** 权限被拒：弹窗引导，能跳设置就跳 */
  private askPermissionSetting(): void {
    this.getUIContext().showAlertDialog({
      title: '需要相册权限',
      message: '保存报告图需要“访问图片和视频”权限。点“去设置”后在权限页打开它；也可以直接点“分享给家人”把图发出去。',
      autoCancel: true,
      primaryButton: {
        value: '去设置',
        action: () => {
          const ctx: common.UIAbilityContext =
            this.getUIContext().getHostContext() as common.UIAbilityContext;
          ReportExporter.openPermissionSetting(ctx);
        }
      },
      secondaryButton: {
        value: '知道了',
        action: () => {
        }
      }
    });
  }

  /**
   * 长图内容：顶部标题、中间图表、底部落款。
   * 固定 720vp 宽，白色底，保证导出后在任何相册/文档里都清楚。
   */
  @Builder
  longReport() {
    Column() {
      // ---- 标题区 ----
      Row() {
        Column() {
          Text('守夜灯 · 居家安全报告').fontSize(26).fontWeight(FontWeight.Bold).fontColor('#0D1326')
          Text(this.daily === null ? '数据生成中' : this.daily.date + ' · 独居老人居家安全监测')
            .fontSize(13).fontColor('#64748B').margin({ top: 6 })
        }
        .alignItems(HorizontalAlign.Start)
        .layoutWeight(1)
        Text(this.levelText())
          .fontSize(14).fontColor('#FFFFFF').backgroundColor(this.levelColor())
          .borderRadius(14).padding({ left: 12, right: 12, top: 6, bottom: 6 })
      }
      .width('100%')
      .alignItems(VerticalAlign.Center)
      .padding({ bottom: 14 })
      .border({ width: 0, color: '#FFFFFF' })

      Divider().color('#E2E8F0')

      // ---- 今日概览 ----
      Row({ space: 12 }) {
        this.reportMetric('今日活动', this.daily === null ? '—' : this.daily.totalCount.toString() + ' 次',
          this.daily === null || this.daily.deltaVsYesterday === null ? '较昨日 暂无对比'
            : '较昨日 ' + (this.daily.deltaVsYesterday >= 0 ? '+' : '') + this.daily.deltaVsYesterday + '%')
        this.reportMetric('本周日均', this.weekly === null ? '—' : this.weekly.dailyAvg.toString() + ' 次',
          this.weekly === null || this.weekly.deltaVsLastWeek === null ? '较上周 暂无对比'
            : '较上周 ' + (this.weekly.deltaVsLastWeek >= 0 ? '+' : '') + this.weekly.deltaVsLastWeek + '%')
        this.reportMetric('本月告警', this.monthly === null ? '—' : this.monthly.alarmCount.toString() + ' 条',
          this.monthly === null ? '' : '统计 ' + this.monthly.days.toString() + ' 天')
      }
      .width('100%')
      .margin({ top: 16 })

      // ---- 近 7 日活动柱状 ----
      Column() {
        Text('近 7 天活动次数').fontSize(14).fontColor('#334155').width('100%')
        Row() {
          ForEach(this.repWeekBars(), (item: number, index: number) => {
            Column() {
              Text(item.toString()).fontSize(11).fontColor('#64748B')
              Column()
                .width(28)
                .height(this.repBarHeight(item))
                .backgroundColor('#1D4ED8')
                .borderRadius({ topLeft: 6, topRight: 6 })
                .margin({ top: 4 })
              Text(this.repWeekLabel(index)).fontSize(11).fontColor('#94A3B8').margin({ top: 4 })
            }
            .layoutWeight(1)
            .alignItems(HorizontalAlign.Center)
          }, (item: number, index: number) => index.toString())
        }
        .width('100%')
        .alignItems(VerticalAlign.Bottom)
        .margin({ top: 10 })
      }
      .width('100%')
      .alignItems(HorizontalAlign.Start)
      .margin({ top: 20 })

      // ---- 环境 ----
      Row({ space: 12 }) {
        this.reportMetric('平均温度',
          this.weekly === null || this.weekly.tempAvg === null ? '—' : this.weekly.tempAvg.toFixed(1) + '℃',
          this.temps.length > 0 ? '最近 ' + this.temps.length.toString() + ' 帧均值' : '')
        this.reportMetric('平均湿度',
          this.weekly === null || this.weekly.humAvg === null ? '—' : this.weekly.humAvg.toFixed(0) + '%',
          '')
        this.reportMetric('今日告警', this.daily === null ? '—' : this.daily.alarmCount.toString() + ' 条', '')
      }
      .width('100%')
      .margin({ top: 20 })

      // ---- 结论 ----
      Text(this.conclusionText())
        .fontSize(13)
        .fontColor('#334155')
        .backgroundColor('#F1F5F9')
        .borderRadius(10)
        .padding(12)
        .width('100%')
        .margin({ top: 18 })

      // ---- 落款 ----
      Divider().color('#E2E8F0').margin({ top: 18 })
      Column() {
        Text('守夜灯 · 独居老人居家安全监测系统').fontSize(12).fontColor('#64748B')
        Text('生成时间 ' + this.stampText() + '　|　网关 ' + Settings.baseUrl)
          .fontSize(11).fontColor('#94A3B8').margin({ top: 4 })
        Text('数据来源：' + this.sourceText() + '　|　本图由 APP 本地生成，未上传网络')
          .fontSize(11).fontColor('#94A3B8').margin({ top: 4 })
      }
      .alignItems(HorizontalAlign.Start)
      .width('100%')
      .margin({ top: 10 })
    }
    .width(720)
    .padding(26)
    .backgroundColor('#FFFFFF')
    .alignItems(HorizontalAlign.Start)
  }

  @Builder
  reportMetric(title: string, value: string, sub: string) {
    Column() {
      Text(title).fontSize(12).fontColor('#64748B')
      Text(value).fontSize(22).fontWeight(FontWeight.Bold).fontColor('#0D1326').margin({ top: 2 })
      Text(sub.length > 0 ? sub : ' ').fontSize(11).fontColor('#94A3B8').margin({ top: 2 })
    }
    .alignItems(HorizontalAlign.Start)
    .layoutWeight(1)
    .padding(12)
    .backgroundColor('#F8FAFC')
    .borderRadius(10)
  }

  private repWeekBars(): number[] {
    return this.weekly === null ? [] : this.weekly.dailyCounts;
  }

  private repBarHeight(v: number): number {
    let max: number = 1;
    const arr: number[] = this.repWeekBars();
    for (let i = 0; i < arr.length; i++) {
      if (arr[i] > max) {
        max = arr[i];
      }
    }
    return 12 + Math.round(58 * v / max);
  }

  private repWeekLabel(index: number): string {
    const names: string[] = ['一', '二', '三', '四', '五', '六', '日'];
    return names[index % 7];
  }

  private levelText(): string {
    const lv: string = this.daily === null ? '' : this.daily.level;
    if (lv === 'L3') {
      return '紧急';
    }
    if (lv === 'L2') {
      return '关注';
    }
    if (lv === 'L1') {
      return '提醒';
    }
    return '正常';
  }

  private levelColor(): string {
    const lv: string = this.daily === null ? '' : this.daily.level;
    if (lv === 'L3') {
      return '#DC2626';
    }
    if (lv === 'L2') {
      return '#EA580C';
    }
    if (lv === 'L1') {
      return '#2563EB';
    }
    return '#059669';
  }

  private conclusionText(): string {
    if (this.daily === null) {
      return '结论：数据不足，暂无结论';
    }
    if (this.daily.note.length > 0) {
      return '结论：' + this.daily.note;
    }
    return '结论：近 7 天活动平稳，未发现需要关注的异常。';
  }

  private stampText(): string {
    const d: Date = new Date();
    const p2 = (n: number): string => (n < 10 ? '0' + n.toString() : n.toString());
    return d.getFullYear() + '-' + p2(d.getMonth() + 1) + '-' + p2(d.getDate()) +
      ' ' + p2(d.getHours()) + ':' + p2(d.getMinutes());
  }

  private sourceText(): string {
    if (this.daily !== null && this.daily.date.length > 0) {
      return '测试/模拟（非真实传感器采集）';
    }
    return '未知';
  }

  /** 全屏预览：半透明黑底 + 居中卡片 + 双指缩放 + 底部两个大按钮 */
  @Builder
  previewOverlay() {
    Column() {
      // 顶部：标题 + 明显的关闭按钮
      Row() {
        Text('报告图预览').fontSize(Theme.fs(20, this.largeFont)).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
        Blank()
        Text('✕')
          .fontSize(26)
          .fontWeight(FontWeight.Bold)
          .fontColor('#FFFFFF')
          .width(56)
          .height(56)
          .textAlign(TextAlign.Center)
          .borderRadius(28)
          .backgroundColor('rgba(255,255,255,0.18)')
          .onClick(() => {
            this.showPreview = false;
            AppStorage.setOrCreate('reportPreviewOpen', false);
          })
      }
      .width('100%')
      .padding({ left: 18, right: 18, top: 18, bottom: 10 })

      // 中间：长图卡片（可双指缩放 / 放大后拖动）
      Stack() {
        if (this.previewPm !== null) {
          Column() {
            Image(this.previewPm)
              .objectFit(ImageFit.Contain)
              .width('100%')
              .borderRadius(12)
              .scale({ x: this.zoom, y: this.zoom })
              .translate({ x: this.panX, y: this.panY })
          }
          .width('100%')
          .padding(14)
          .backgroundColor('#FFFFFF')
          .borderRadius(16)
          .margin({ left: 12, right: 12 })
        } else {
          Column() {
            LoadingProgress().width(56).height(56).color('#FFFFFF')
            Text('正在生成报告图…').fontSize(Theme.fs(16, this.largeFont)).fontColor('#FFFFFF').margin({ top: 12 })
          }
          .justifyContent(FlexAlign.Center)
          .height('100%')
        }
      }
      .layoutWeight(1)
      .width('100%')
      .clip(true)
      .gesture(
        GestureGroup(GestureMode.Parallel,
          PinchGesture({ fingers: 2 })
            .onActionUpdate((event: GestureEvent) => {
              const next: number = this.baseZoom * event.scale;
              this.zoom = next < 1 ? 1 : (next > 4 ? 4 : next);
            })
            .onActionEnd(() => {
              this.baseZoom = this.zoom;
              if (this.zoom <= 1) {
                this.panX = 0;
                this.panY = 0;
              }
            }),
          PanGesture({ fingers: 2 })
            .onActionUpdate((event: GestureEvent) => {
              if (this.zoom > 1) {
                this.panX = this.panX + event.offsetX;
                this.panY = this.panY + event.offsetY;
              }
            })
            .onActionEnd(() => {
            })
        )
      )

      if (this.saveTip.length > 0) {
        Text(this.saveTip)
          .fontSize(Theme.fs(13, this.largeFont))
          .fontColor('#FDE68A')
          .width('100%')
          .textAlign(TextAlign.Center)
          .padding({ left: 18, right: 18, bottom: 6 })
      }

      // 底部：两个大按钮（适老化，高度 60vp）
      Row({ space: 12 }) {
        Button(this.saving ? '保存中…' : '保存到相册')
          .fontSize(Theme.fs(18, this.largeFont))
          .fontWeight(FontWeight.Bold)
          .fontColor('#FFFFFF')
          .backgroundColor(this.saving ? '#64748B' : Theme.blue)
          .height(60)
          .layoutWeight(1)
          .borderRadius(14)
          .enabled(!this.saving && this.previewPm !== null)
          .onClick(() => {
            this.doSave();
          })
        Button('分享给家人')
          .fontSize(Theme.fs(18, this.largeFont))
          .fontWeight(FontWeight.Bold)
          .fontColor('#0D1326')
          .backgroundColor('#FFFFFF')
          .height(60)
          .layoutWeight(1)
          .borderRadius(14)
          .enabled(this.previewPm !== null)
          .onClick(() => {
            this.doShare();
          })
      }
      .width('100%')
      .padding({ left: 18, right: 18, top: 6, bottom: 28 })
    }
    .width('100%')
    .height('100%')
    .backgroundColor('rgba(0,0,0,0.78)')
  }

  async load(): Promise<void> {
    const now: Date = new Date();
    const y: string = now.getFullYear().toString();
    const m: string = (now.getMonth() + 1).toString().padStart(2, '0');
    const d: string = now.getDate().toString().padStart(2, '0');
    this.daily = await ApiClient.daily(`${y}-${m}-${d}`);
    this.weekly = await ApiClient.weekly();

    // 月报：只提供“本月/上月”两个入口；monthOffset 变化由 switchMonth() 触发重取
    let my: number = now.getFullYear();
    let mm: number = now.getMonth() + 1 - this.monthOffset;
    while (mm <= 0) {
      mm += 12;
      my -= 1;
    }
    this.monthKey = `${my}-${mm.toString().padStart(2, '0')}`;
    this.monthly = await ApiClient.monthly(this.monthKey);

    const frames: SensorSnapshot[] | null = await ApiClient.snapshots(24);
    const series: number[] = [];
    if (frames !== null) {
      for (let i = 0; i < frames.length; i++) {
        const t: number | null = frames[i].temperature;
        if (t !== null) {
          series.push(t);
        }
      }
    }
    this.temps = series;
    this.loaded = true;
  }

  bins(): number[] {
    const result: number[] = [];
    if (this.daily === null) {
      return result;
    }
    for (let i = 0; i < 12; i++) {
      const a: number = this.daily.hourly[i * 2];
      const b: number = this.daily.hourly[i * 2 + 1];
      result.push(a + b);
    }
    return result;
  }

  maxBin(): number {
    const data: number[] = this.bins();
    let max: number = 1;
    for (let i = 0; i < data.length; i++) {
      if (data[i] > max) {
        max = data[i];
      }
    }
    return max;
  }

  barHeight(value: number): string {
    const ratio: number = value / this.maxBin();
    return `${Math.max(3, Math.round(ratio * 100))}%`;
  }

  tempHeight(value: number): string {
    let min: number = 100;
    let max: number = -100;
    for (let i = 0; i < this.temps.length; i++) {
      if (this.temps[i] < min) {
        min = this.temps[i];
      }
      if (this.temps[i] > max) {
        max = this.temps[i];
      }
    }
    const span: number = max - min;
    const ratio: number = span < 0.5 ? 0.5 : (value - min) / span;
    return `${Math.max(8, Math.round(ratio * 100))}%`;
  }

  /** 整数或“—”：月报整块取不到时传 null */
  countText(value: number | null): string {
    return value === null ? '—' : value.toString();
  }

  /** 一位小数 + 单位，null 显示“—”而不是 0，避免误导成“当月均温 0℃” */
  fixedText(value: number | null, unit: string): string {
    return value === null ? '—' : value.toFixed(1) + unit;
  }

  /** 切换本月/上月；同一档重复点不重取 */
  switchMonth(offset: number): void {
    if (this.monthOffset === offset) {
      return;
    }
    this.monthOffset = offset;
    this.load();
  }

  @Builder
  mainBody() {
    Column() {
      Row() {
        Text('报告').fontSize(Theme.fs(20, this.largeFont)).fontWeight(FontWeight.Bold).fontColor('#FFFFFF')
        Text(this.exporting ? '生成中…' : '导出图')
          .fontSize(Theme.fs(12, this.largeFont))
          .fontColor(Theme.blueLight)
          .padding({ left: 12, right: 12, top: 6, bottom: 6 })
          .backgroundColor('rgba(255,255,255,0.14)')
          .borderRadius(10)
          .margin({ right: 8 })
          .onClick(() => {
            this.openPreview();
          })
        Blank()
        Text(this.daily === null ? '—' : this.daily.date).fontSize(Theme.fs(12, this.largeFont)).fontColor(Theme.blueLight)
      }
      .width('100%')
      .padding(Theme.page)
      .backgroundColor(Theme.navy)

      Scroll() {
        Column({ space: Theme.gap }) {
          if (this.exportTip.length > 0) {
            Text(this.exportTip)
              .fontSize(Theme.fs(11, this.largeFont))
              .fontColor(this.exportTip.indexOf('已导出') >= 0 ? Theme.green : Theme.muted)
              .width('100%')
          }

          // 今日活动
          Column() {
            Text('今日活动').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted).width('100%')
            Row() {
              Text(this.daily === null ? '—' : this.daily.totalCount.toString())
                .fontSize(Theme.fs(56, this.largeFont))
                .fontWeight(FontWeight.Bold)
                .fontColor(Theme.navy)
              Text('次').fontSize(Theme.fs(14, this.largeFont)).fontColor(Theme.muted).margin({ left: 6, top: 18 })
              Blank()
              if (this.daily !== null && this.daily.deltaVsYesterday !== null) {
                Text(`${this.daily.deltaVsYesterday >= 0 ? '+' : ''}${this.daily.deltaVsYesterday}%`)
                  .fontSize(Theme.fs(13, this.largeFont))
                  .fontColor(this.daily.deltaVsYesterday >= 0 ? Theme.green : Theme.amber)
                  .backgroundColor('#FEF3C7')
                  .borderRadius(12)
                  .padding({ left: 10, right: 10, top: 5, bottom: 5 })
              }
            }
            .width('100%')
            .alignItems(VerticalAlign.Bottom)

            Text(this.daily === null ? '暂无数据'
              : (this.daily.deltaVsYesterday === null ? '较昨日：暂无对比' : '较昨日 ' + this.daily.deltaVsYesterday + '%'))
              .fontSize(Theme.fs(12, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 4 })
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 时段分布
          Column() {
            Text('活动时段分布（每 2 小时）').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted).width('100%')

            Row({ space: 4 }) {
              ForEach(this.bins(), (value: number, index: number) => {
                Column() {
                  Blank()
                  Column()
                    .width('100%')
                    .height(this.barHeight(value))
                    .backgroundColor(index === 4 ? Theme.orange : Theme.blueLight)
                    .borderRadius(4)
                }
                .layoutWeight(1)
                .height(120)
                .justifyContent(FlexAlign.End)
              }, (value: number, index: number) => index.toString())
            }
            .width('100%')
            .height(130)
            .margin({ top: 10 })
            .alignItems(VerticalAlign.Bottom)

            Row() {
              Text('0').fontSize(Theme.fs(10, this.largeFont)).fontColor(Theme.muted)
              Blank()
              Text('12').fontSize(Theme.fs(10, this.largeFont)).fontColor(Theme.muted)
              Blank()
              Text('24').fontSize(Theme.fs(10, this.largeFont)).fontColor(Theme.muted)
            }
            .width('100%')
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 基线提示
          Column() {
            Row() {
              Text('7 日基线').fontSize(Theme.fs(13, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.blue)
              Blank()
              Text(this.daily !== null && this.daily.baseline !== null
                ? `均值 ${this.daily.baseline} 次` : '数据不足')
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.muted)
            }
            .width('100%')

            Text(this.daily === null || this.daily.note.length === 0
              ? '连续 3 日低于基线 30% 才会提示，单日波动不打扰。'
              : this.daily.note)
              .fontSize(Theme.fs(13, this.largeFont))
              .fontColor(Theme.slate)
              .width('100%')
              .margin({ top: 8 })

            Text('※ 提示非诊断，仅供家属参考')
              .fontSize(Theme.fs(10, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 6 })
          }
          .width('100%')
          .padding(16)
          .backgroundColor('#EFF6FF')
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.blueLight })

          // 环境趋势（温度）
          Column() {
            Row() {
              Text('环境趋势（最近 24 帧温度）').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted)
              Blank()
              Text(this.temps.length === 0 ? '无数据'
                : `${this.temps[this.temps.length - 1].toFixed(1)}℃`)
                .fontSize(Theme.fs(12, this.largeFont))
                .fontColor(Theme.navy)
            }
            .width('100%')

            if (this.temps.length === 0) {
              Text('尚未采集到温度数据（传感器未接入时该字段为 null）')
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.muted)
                .width('100%')
                .margin({ top: 10 })
            } else {
              Row({ space: 3 }) {
                ForEach(this.temps, (value: number) => {
                  Column()
                    .layoutWeight(1)
                    .height(this.tempHeight(value))
                    .backgroundColor(Theme.blue)
                    .borderRadius(3)
                }, (value: number, index: number) => index.toString())
              }
              .width('100%')
              .height(70)
              .alignItems(VerticalAlign.Bottom)
              .margin({ top: 10 })
            }
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 本周
          Column() {
            Text('本周概览').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted).width('100%')
            Row() {
              Column() {
                Text(this.weekly === null ? '—' : this.weekly.totalCount.toString())
                  .fontSize(Theme.fs(22, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.navy)
                Text('总活动').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }.layoutWeight(1)

              Column() {
                Text(this.weekly === null ? '—' : this.weekly.dailyAvg.toString())
                  .fontSize(Theme.fs(22, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.navy)
                Text('日均').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }.layoutWeight(1)

              Column() {
                Text(this.weekly === null ? '—' : this.weekly.alarmCount.toString())
                  .fontSize(Theme.fs(22, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.navy)
                Text('告警').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }.layoutWeight(1)
            }
            .width('100%')
            .margin({ top: 12 })
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          // 月度报告概览
          Column() {
            Row() {
              Text('月度概览').fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted)
              Blank()
              Text(this.monthKey.length === 0 ? '—' : this.monthKey)
                .fontSize(Theme.fs(11, this.largeFont))
                .fontColor(Theme.muted)
              Row({ space: 6 }) {
                Text('本月')
                  .fontSize(Theme.fs(11, this.largeFont))
                  .fontColor(this.monthOffset === 0 ? '#FFFFFF' : Theme.blue)
                  .backgroundColor(this.monthOffset === 0 ? Theme.blue : '#EFF6FF')
                  .borderRadius(10)
                  .padding({ left: 10, right: 10, top: 4, bottom: 4 })
                  .onClick(() => {
                    this.switchMonth(0);
                  })
                Text('上月')
                  .fontSize(Theme.fs(11, this.largeFont))
                  .fontColor(this.monthOffset === 1 ? '#FFFFFF' : Theme.blue)
                  .backgroundColor(this.monthOffset === 1 ? Theme.blue : '#EFF6FF')
                  .borderRadius(10)
                  .padding({ left: 10, right: 10, top: 4, bottom: 4 })
                  .onClick(() => {
                    this.switchMonth(1);
                  })
              }
              .margin({ left: 8 })
            }
            .width('100%')

            Row() {
              Column() {
                Text(this.monthly === null ? '—' : this.monthly.totalCount.toString())
                  .fontSize(Theme.fs(22, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.navy)
                Text('总活动').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }
              .layoutWeight(1)

              Column() {
                Text(this.monthly === null ? '—' : this.monthly.dailyAvg.toFixed(1))
                  .fontSize(Theme.fs(22, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.navy)
                Text('日均').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }
              .layoutWeight(1)

              Column() {
                Text(this.monthly === null ? '—' : this.countText(this.monthly.alarmCount))
                  .fontSize(Theme.fs(22, this.largeFont)).fontWeight(FontWeight.Bold).fontColor(Theme.navy)
                Text('告警').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }
              .layoutWeight(1)
            }
            .width('100%')
            .margin({ top: 12 })

            Row() {
              Column() {
                Text(this.monthly === null ? '—' : this.fixedText(this.monthly.tempAvg, '℃'))
                  .fontSize(Theme.fs(18, this.largeFont)).fontWeight(FontWeight.Medium).fontColor(Theme.slate)
                Text('平均温度').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }
              .layoutWeight(1)

              Column() {
                Text(this.monthly === null ? '—' : this.fixedText(this.monthly.humAvg, '%'))
                  .fontSize(Theme.fs(18, this.largeFont)).fontWeight(FontWeight.Medium).fontColor(Theme.slate)
                Text('平均湿度').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }
              .layoutWeight(1)

              Column() {
                Text(this.monthly === null ? '—' : `${this.monthly.days} 天`)
                  .fontSize(Theme.fs(18, this.largeFont)).fontWeight(FontWeight.Medium).fontColor(Theme.slate)
                Text('当月天数').fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 4 })
              }
              .layoutWeight(1)
            }
            .width('100%')
            .margin({ top: 14 })

            Text(this.monthly === null
              ? '未取到月报数据（网关离线或当月无记录）：显示“—”而不是 0'
              : '※ 口径与今日/本周一致；温湿度为当月均值，无采集时显示“—”')
              .fontSize(Theme.fs(10, this.largeFont))
              .fontColor(Theme.muted)
              .width('100%')
              .margin({ top: 10 })
          }
          .width('100%')
          .padding(16)
          .backgroundColor(Theme.card)
          .borderRadius(Theme.radius)
          .border({ width: 1, color: Theme.line })

          Text(this.loaded && this.daily === null ? '⚠ 未取到数据，请检查网关连接' : '')
            .fontSize(Theme.fs(11, this.largeFont))
            .fontColor(Theme.orange)
        }
        .padding(Theme.page)
      }
      .layoutWeight(1)
      .scrollBar(BarState.Off)
    }
    .width('100%')
    .height('100%')
    .backgroundColor(Theme.bg)
    .id('reportSnapshot')
  }

  /** 根视图：内容 + 全屏导出预览叠层（不用 bindContentCover，方便调试与截图） */
  build() {
    Stack() {
      this.mainBody();
      if (this.showPreview) {
        this.previewOverlay();
      }
    }
    .width('100%')
    .height('100%')
  }

}
