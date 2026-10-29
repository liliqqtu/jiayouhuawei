/**
 * 设计令牌：与路演 PPT / 设计文档保持同一套色板。
 * 颜色集中在这里，避免各页面散落魔法值。
 */
export class Theme {
  static readonly bg: string = '#F2F4F8';
  static readonly card: string = '#FFFFFF';
  static readonly navy: string = '#0D1326';
  static readonly blue: string = '#2563EB';
  static readonly blueLight: string = '#60A5FA';
  static readonly orange: string = '#EA580C';
  static readonly green: string = '#10B981';
  static readonly red: string = '#B91C1C';
  static readonly amber: string = '#B45309';
  static readonly slate: string = '#334155';
  static readonly muted: string = '#64748B';
  /** 高对比文字（关怀模式下替代 muted，弱视/色弱老人看得更清） */
  static readonly strong: string = '#0B1220';
  static readonly line: string = '#DCE3EC';

  static readonly radius: number = 16;
  static readonly gap: number = 12;
  static readonly page: number = 16;

  /**
   * 关怀模式（大字）：字号整体放大倍率。
   * 取 1.2 是实测出来的：正文 13 -> 16px、标题 20 -> 24px，老人隔半米能看清，
   * 又不会把一张卡片撑成两屏（1.3 以上「我的」页会溢出需要滚动）。
   * 注意：字号只在这里放大一次，各页面统一走 Theme.fs()，不各写各的。
   */
  static readonly largeScale: number = 1.2;

  /** 字号换算：large 为真时按倍率放大并取整（ArkUI 的 fontSize 只吃 number） */
  static fs(base: number, large: boolean): number {
    return large ? Math.round(base * Theme.largeScale) : base;
  }

  /** 告警等级色 */
  static levelColor(level: string): string {
    if (level === 'L3') {
      return Theme.red;
    }
    if (level === 'L2') {
      return Theme.orange;
    }
    return Theme.blueLight;
  }
}
