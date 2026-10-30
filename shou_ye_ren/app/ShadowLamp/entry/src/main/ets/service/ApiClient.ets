/**
 * 网络层 + 本地设置。
 *
 * 接口请求与本地参数保存
 */
import { http } from '@kit.NetworkKit';
import { preferences } from '@kit.ArkData';
import { common } from '@kit.AbilityKit';
import { AlarmEvent, AlarmListResult, DailyReport, DeviceAddResult, DeviceListResult, DeviceStatus, FamilyInfo, MonthlyReport, SensorSnapshot, WeeklyReport } from '../model/Models';

const PREF_NAME: string = 'shadowlamp';
const KEY_BASE: string = 'baseUrl';
export const DEFAULT_BASE: string = 'http://10.179.139.87:8780';
/** 默认家人电话（演示用；真机部署时在「我的」页或本常量里改成真实号码） */
export const DEFAULT_FAMILY_PHONE: string = '13800138000';

interface AckBody {
  operator: string;
}

interface CloseBody {
  handleType: string;
  handleResult: string;
  operator: string;
}

/** 创建家庭组请求体 */
interface CreateFamilyBody {
  familyName: string;
  ownerName: string;
}

/** 加入家庭组请求体 */
interface JoinFamilyBody {
  inviteCode: string;
  memberName: string;
}

/** 网关错误体：{ code, message } */
interface FamilyErrorBody {
  code: string;
  message: string;
}

/** 家庭组结果：把“成功与否 + 原因”一起带出来，界面据此给准确提示 */
export interface FamilyResult {
  ok: boolean;
  info: FamilyInfo | null;
  code: string;
  message: string;
}

/** 登记设备请求体 */
interface RegisterBody {
  deviceId: string;
}

interface SosZone {
  zone: string;
}

interface SosBody {
  deviceId: string;
  ts: string;
  type: string;
  source: string;
  payload: SosZone;
}

export class Settings {
  static baseUrl: string = DEFAULT_BASE;
  private static pref: preferences.Preferences | null = null;
  private static ctx: common.UIAbilityContext | null = null;

  static async init(context: common.UIAbilityContext): Promise<void> {
    Settings.ctx = context;
    try {
      Settings.pref = await preferences.getPreferences(context, PREF_NAME);
      const saved: preferences.ValueType = await Settings.pref.get(KEY_BASE, DEFAULT_BASE);
      const text: string = saved as string;
      Settings.baseUrl = text.length > 0 ? text : DEFAULT_BASE;
      const seen: preferences.ValueType = await Settings.pref.get('guideDone', false);
      Settings.guideDone = seen as boolean;
      const big: preferences.ValueType = await Settings.pref.get('largeFont', false);
      Settings.largeFont = big as boolean;
      const dev: preferences.ValueType = await Settings.pref.get('deviceId', '');
      Settings.deviceId = dev as string;
      const phone: preferences.ValueType = await Settings.pref.get('familyPhone', DEFAULT_FAMILY_PHONE);
      const phoneText: string = phone as string;
      Settings.familyPhone = phoneText.length > 0 ? phoneText : DEFAULT_FAMILY_PHONE;
    } catch (err) {
      Settings.baseUrl = DEFAULT_BASE;
    }
    // 同步到 AppStorage，界面用 @StorageLink 就能立刻刷新
    AppStorage.setOrCreate<string>('baseUrl', Settings.baseUrl);
    AppStorage.setOrCreate<boolean>('largeFont', Settings.largeFont);
    AppStorage.setOrCreate<string>('deviceId', Settings.deviceId);
    AppStorage.setOrCreate<string>('familyPhone', Settings.familyPhone);
  }

  /** 是否已看过新手引导 */
  static guideDone: boolean = false;

  /** 关怀模式（大字）：默认关闭，改写后所有页面字号同步放大 */
  static largeFont: boolean = false;

  /**
   * 当前选中的设备。空串表示"不指定"——网关返回最近上报的那台。
   * 多老人场景（父母两台设备）在「我的」页切换，选择会持久化。
   */
  static deviceId: string = '';
  /** 家人电话：「告警详情页」的一键呼叫按钮用它；空串时不拨号并提示去设置 */
  static familyPhone: string = DEFAULT_FAMILY_PHONE;

  static async setDevice(deviceId: string): Promise<void> {
    Settings.deviceId = deviceId;
    AppStorage.setOrCreate<string>('deviceId', deviceId);
    if (Settings.pref === null && Settings.ctx !== null) {
      try {
        Settings.pref = await preferences.getPreferences(Settings.ctx, PREF_NAME);
      } catch (err) {
        Settings.pref = null;
      }
    }
    if (Settings.pref !== null) {
      await Settings.pref.put('deviceId', deviceId);
      await Settings.pref.flush();
    }
  }

  static async setFamilyPhone(phone: string): Promise<void> {
    Settings.familyPhone = phone;
    AppStorage.setOrCreate<string>('familyPhone', phone);
    if (Settings.pref !== null) {
      await Settings.pref.put('familyPhone', phone);
      await Settings.pref.flush();
    }
  }

  static async setLargeFont(on: boolean): Promise<void> {
    Settings.largeFont = on;
    AppStorage.setOrCreate<boolean>('largeFont', on);
    if (Settings.pref !== null) {
      await Settings.pref.put('largeFont', on);
      await Settings.pref.flush();
    }
  }

  static async markGuideDone(): Promise<void> {
    Settings.guideDone = true;
    if (Settings.pref !== null) {
      await Settings.pref.put('guideDone', true);
      await Settings.pref.flush();
    }
  }

  static async save(url: string): Promise<void> {
    Settings.baseUrl = url;
    AppStorage.setOrCreate<string>('baseUrl', url);

    // 兜底：如果 init 还没跑完（pref 为空），先用备用 context 补一次初始化
    if (Settings.pref === null && Settings.ctx !== null) {
      try {
        Settings.pref = await preferences.getPreferences(Settings.ctx, PREF_NAME);
      } catch (err) {
        Settings.pref = null;
      }
    }
    if (Settings.pref !== null) {
      await Settings.pref.put(KEY_BASE, url);
      await Settings.pref.flush();
    }
  }
}

export class ApiClient {
  // ---------------------------------------------------------------- 家庭组
  /**
   * 创建家庭组：一位老人一个组，创建者自动是“户主”，网关返回 6 位加入密码。
   * 家庭组存在网关内存里（重启重置）——本期不搭云，这是有意的取舍。
   */
  static async createFamily(familyName: string, ownerName: string): Promise<FamilyResult> {
    const body: CreateFamilyBody = { familyName: familyName, ownerName: ownerName };
    return ApiClient.sendFamily('/api/v1/family/create', http.RequestMethod.POST,
      JSON.stringify(body));
  }

  /** 用 6 位密码加入家庭组，身份是“监护人” */
  static async joinFamily(inviteCode: string, memberName: string): Promise<FamilyResult> {
    const body: JoinFamilyBody = { inviteCode: inviteCode, memberName: memberName };
    return ApiClient.sendFamily('/api/v1/family/join', http.RequestMethod.POST,
      JSON.stringify(body));
  }

  /** 按 familyId 拉家庭组详情（成员、密码），重开 APP 后刷新成员用 */
  static async familyInfo(familyId: string): Promise<FamilyResult> {
    return ApiClient.sendFamily(`/api/v1/family?familyId=${familyId}`,
      http.RequestMethod.GET, '');
  }

  /**
   * 家庭组专用的请求封装：不吞错误码。
   * 普通接口失败返回空串就够（页面显示“离线”），但加入家庭组必须告诉用户
   * “密码不对（404）”还是“连不上网关”，所以单独走这套。
   */
  private static async sendFamily(path: string, method: http.RequestMethod,
                                  body: string): Promise<FamilyResult> {
    const url: string = Settings.baseUrl + path;
    const req = http.createHttp();
    try {
      const options: http.HttpRequestOptions = {
        method: method,
        connectTimeout: 4000,
        readTimeout: 4000,
        expectDataType: http.HttpDataType.STRING
      };
      if (body.length > 0) {
        options.header = { 'Content-Type': 'application/json' };
        options.extraData = body;
      }
      const resp: http.HttpResponse = await req.request(url, options);
      const text: string = resp.result as string;
      if (resp.responseCode === 200) {
        const info: FamilyInfo = JSON.parse(text) as FamilyInfo;
        return { ok: true, info: info, code: '', message: '' };
      }
      // 网关的错误体是 { code, message }；解析失败就按 HTTP 码兜底
      try {
        const err: FamilyErrorBody = JSON.parse(text) as FamilyErrorBody;
        return { ok: false, info: null, code: err.code, message: err.message };
      } catch (inner) {
        return { ok: false, info: null, code: 'HTTP_' + resp.responseCode,
          message: '网关返回 ' + resp.responseCode };
      }
    } catch (err) {
      return { ok: false, info: null, code: 'NETWORK', message: '连不上网关，请检查网关地址与同一局域网' };
    } finally {
      req.destroy();
    }
  }

  private static async request(path: string, method: http.RequestMethod, body: string): Promise<string> {
    const url: string = Settings.baseUrl + path;
    const req = http.createHttp();
    try {
      const options: http.HttpRequestOptions = {
        method: method,
        connectTimeout: 4000,
        readTimeout: 4000,
        expectDataType: http.HttpDataType.STRING
      };
      if (body.length > 0) {
        options.header = { 'Content-Type': 'application/json' };
        options.extraData = body;
      }
      const resp: http.HttpResponse = await req.request(url, options);
      if (resp.responseCode !== 200) {
        return '';
      }
      return resp.result as string;
    } catch (err) {
      return '';
    } finally {
      req.destroy();
    }
  }

  /** 下发个性化阈值（原始 JSON 字符串，避免 ArkTS 对象字面量限制） */
  static async applyConfig(json: string): Promise<boolean> {
    const text: string = await ApiClient.request('/api/v1/config/threshold', http.RequestMethod.POST, json);
    return text.length > 0;
  }

  static async health(): Promise<boolean> {
    const text: string = await ApiClient.request('/health', http.RequestMethod.GET, '');
    return text.length > 0;
  }

  /** deviceId 不传就用「我的」页选中的设备；都没有则返回最近上报的那台 */
  static async snapshot(deviceId?: string): Promise<SensorSnapshot | null> {
    const id: string = deviceId === undefined ? Settings.deviceId : deviceId;
    let path: string = '/api/v1/snapshot';
    if (id.length > 0) {
      path = path + '?deviceId=' + encodeURIComponent(id);
    }
    const text: string = await ApiClient.request(path, http.RequestMethod.GET, '');
    if (text.length === 0) {
      return null;
    }
    try {
      return JSON.parse(text) as SensorSnapshot;
    } catch (err) {
      return null;
    }
  }

  static async device(): Promise<DeviceStatus | null> {
    const text: string = await ApiClient.request('/api/v1/device', http.RequestMethod.GET, '');
    if (text.length === 0) {
      return null;
    }
    try {
      return JSON.parse(text) as DeviceStatus;
    } catch (err) {
      return null;
    }
  }

  /** 多设备清单；失败返回 null（离线时页面提示"取不到设备列表"） */
  static async devices(): Promise<DeviceStatus[] | null> {
    const text: string = await ApiClient.request('/api/v1/devices', http.RequestMethod.GET, '');
    if (text.length === 0) {
      return null;
    }
    try {
      const result: DeviceListResult = JSON.parse(text) as DeviceListResult;
      return result.items;
    } catch (err) {
      return null;
    }
  }

  /** 失败返回 null（区别于“确实没有告警”的空数组），页面据此切离线模式 */
  /**
   * 告警列表。
   * @param status pending / acknowledged / closed
   * @param deviceId 传空串或省略表示"全部设备"，多老人多设备时看全局
   */
  static async alarms(status: string, deviceId?: string): Promise<AlarmEvent[] | null> {
    let path: string = `/api/v1/alarms?status=${status}&limit=50`;
    if (deviceId !== undefined && deviceId.length > 0) {
      path += `&deviceId=${deviceId}`;
    }
    const text: string = await ApiClient.request(path,
      http.RequestMethod.GET, '');
    if (text.length === 0) {
      return null;
    }
    try {
      const result: AlarmListResult = JSON.parse(text) as AlarmListResult;
      return result.items;
    } catch (err) {
      return null;
    }
  }

  static async ack(eventId: string): Promise<boolean> {
    const payload: AckBody = { operator: 'family' };
    const body: string = JSON.stringify(payload);
    const text: string = await ApiClient.request(`/api/v1/alarms/${eventId}/ack`,
      http.RequestMethod.POST, body);
    return text.length > 0;
  }

  static async close(eventId: string, handleType: string, result: string): Promise<boolean> {
    const payload: CloseBody = {
      handleType: handleType,
      handleResult: result,
      operator: 'family'
    };
    const body: string = JSON.stringify(payload);
    const text: string = await ApiClient.request(`/api/v1/alarms/${eventId}/close`,
      http.RequestMethod.POST, body);
    return text.length > 0;
  }

  static async daily(date: string): Promise<DailyReport | null> {
    const text: string = await ApiClient.request(`/api/v1/reports/daily?date=${date}`,
      http.RequestMethod.GET, '');
    if (text.length === 0) {
      return null;
    }
    try {
      return JSON.parse(text) as DailyReport;
    } catch (err) {
      return null;
    }
  }

  static async weekly(): Promise<WeeklyReport | null> {
    const text: string = await ApiClient.request('/api/v1/reports/weekly',
      http.RequestMethod.GET, '');
    if (text.length === 0) {
      return null;
    }
    try {
      return JSON.parse(text) as WeeklyReport;
    } catch (err) {
      return null;
    }
  }

  /**
   * 月报：month 形如 2026-10；不传则由网关按当前月份返回。
   * 与 weekly() 一致：请求失败返回 null，不抛异常，由页面显示“—”。
   */
  static async monthly(month?: string): Promise<MonthlyReport | null> {
    let path: string = '/api/v1/reports/monthly';
    if (month !== undefined && month.length > 0) {
      path = `/api/v1/reports/monthly?month=${month}`;
    }
    const text: string = await ApiClient.request(path, http.RequestMethod.GET, '');
    if (text.length === 0) {
      return null;
    }
    try {
      return JSON.parse(text) as MonthlyReport;
    } catch (err) {
      return null;
    }
  }

  /** 历史快照：报告页画环境趋势用 */
  static async snapshots(limit: number): Promise<SensorSnapshot[] | null> {
    const text: string = await ApiClient.request(`/api/v1/snapshots?limit=${limit}`,
      http.RequestMethod.GET, '');
    if (text.length === 0) {
      return null;
    }
    try {
      const obj: Record<string, Object> = JSON.parse(text) as Record<string, Object>;
      return obj['items'] as SensorSnapshot[];
    } catch (err) {
      return null;
    }
  }

  /** 一键求助：走设备事件通道，网关直接建 L3 */
  /**
   * 手工登记一台设备（设备还没上报过数据时，先让它出现在设备列表里）。
   * 只登记编号，不伪造任何传感器数据；它上报第一帧后会自动变“在线”。
   */
  /**
   * 登记一台还没上报过的设备（占位登记）。
   *
   * 为什么不能在前端伪造一条“设备”塞进列表：那样列表里会出现永远在线的假设备，
   * 家属会以为真的有传感器在工作。这里只做登记，设备以“离线/未上报”的真实状态
   * 出现在列表，等它第一帧数据上来才变在线。
   *
   * 三种结果分开返回：
   * - offline：请求抛异常或超时（网关没起、手机和电脑不在同一 WiFi）
   * - bad：网关回 4xx（编号为空 / 含非法字符 / 重名）
   * - ok：已建档
   */
  static async registerDevice(deviceId: string): Promise<DeviceAddResult> {
    const payload: RegisterBody = { deviceId: deviceId };
    const url: string = Settings.baseUrl + '/api/v1/devices';
    const req = http.createHttp();
    try {
      const options: http.HttpRequestOptions = {
        method: http.RequestMethod.POST,
        connectTimeout: 4000,
        readTimeout: 4000,
        expectDataType: http.HttpDataType.STRING,
        header: { 'Content-Type': 'application/json' },
        extraData: JSON.stringify(payload)
      };
      const resp: http.HttpResponse = await req.request(url, options);
      if (resp.responseCode === 200) {
        return { kind: 'ok', message: '' };
      }
      if (resp.responseCode === 400 || resp.responseCode === 409) {
        return { kind: 'bad', message: '设备编号不合规：只能用字母、数字、下划线、点和横线（最长 32 位）' };
      }
      return { kind: 'bad', message: '网关拒绝登记（HTTP ' + resp.responseCode.toString() + '）' };
    } catch (err) {
      const e = err as BusinessError;
      console.error('[device] 登记失败：' + e.message);
      return { kind: 'offline', message: '无法连接网关，请先保存并测试连接' };
    } finally {
      req.destroy();
    }
  }

  static async addDevice(deviceId: string): Promise<boolean> {
    const res: DeviceAddResult = await ApiClient.registerDevice(deviceId);
    return res.kind === 'ok';
  }

  static async sendSos(deviceId: string): Promise<boolean> {
    const now: Date = new Date();
    const zone: SosZone = { zone: 'living' };
    const payload: SosBody = {
      deviceId: deviceId,
      ts: now.toISOString(),
      type: 'SOS',
      source: 'key_sim',
      payload: zone
    };
    const body: string = JSON.stringify(payload);
    const text: string = await ApiClient.request('/api/v1/events', http.RequestMethod.POST, body);
    return text.length > 0;
  }
}
