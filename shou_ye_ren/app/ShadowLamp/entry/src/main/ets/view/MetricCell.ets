/**
 * 指标卡片（独立子组件）。
 *
 * 为什么不用 @Builder：ArkUI 里带参数的 @Builder 是“按值传递”，
 * 父组件状态更新后它不会重新渲染 —— 表现就是数值永远是首帧的 "—"。
 * 用子组件 + @Prop 才能跟着数据变化刷新，这是本项目踩过的真实坑。
 */
import { Theme } from '../common/Theme';

@Component
export struct MetricCell {
  /** 关怀模式（大字）开关，由「我的」页写入 AppStorage */
  @StorageLink('largeFont') largeFont: boolean = false;

  @Prop title: string = '';
  @Prop value: string = '—';
  @Prop hint: string = '';
  /** 越界时由父组件传入警示色 */
  @Prop valueColor: string = Theme.navy;

  build() {
    Column() {
      Text(this.title).fontSize(Theme.fs(13, this.largeFont)).fontColor(Theme.muted)
      Text(this.value)
        .fontSize(Theme.fs(22, this.largeFont))
        .fontWeight(FontWeight.Bold)
        .fontColor(this.valueColor)
        .margin({ top: 6 })
        .maxLines(1)
      Text(this.hint).fontSize(Theme.fs(11, this.largeFont)).fontColor(Theme.muted).margin({ top: 2 })
    }
    .layoutWeight(1)
    .padding({ top: 14, bottom: 14 })
    .backgroundColor(Theme.bg)
    .borderRadius(12)
  }
}
