/**
 * 告警本地持久化（RDB）。
 *
 * 为什么必须有：独居老人场景经常是“网络不好但人要看到历史”，
 * 不能因为网关临时连不上，告警记录就一片空白。
 *
 * 实现取舍：
 *  - 只用一张表 + executeSql 直接写 SQL，不引入 ORM；
 *  - null 统一存成空字符串，读出来再转回 null（ArkTS 的绑定参数对 null 不友好）；
 *  - 不做增量同步，每次拉全量最近 50 条覆盖，简单且不会错。
 */
import { relationalStore } from '@kit.ArkData';
import { common } from '@kit.AbilityKit';
import { AlarmEvent } from '../model/Models';

const CREATE_SQL: string = 'CREATE TABLE IF NOT EXISTS alarm (' +
  'eventId TEXT PRIMARY KEY, ts TEXT, type TEXT, level TEXT, message TEXT, ' +
  'status TEXT, ackTs TEXT, handleType TEXT, handleResult TEXT, closedTs TEXT, ' +
  'zone TEXT, source TEXT, deviceId TEXT)';

const INSERT_SQL: string = 'INSERT OR REPLACE INTO alarm VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)';

export class AlarmStore {
  private static store: relationalStore.RdbStore | null = null;
  private static ready: boolean = false;

  static async init(context: common.UIAbilityContext): Promise<void> {
    if (AlarmStore.ready) {
      return;
    }
    try {
      const config: relationalStore.StoreConfig = {
        name: 'shadowlamp.db',
        securityLevel: relationalStore.SecurityLevel.S1
      };
      const store: relationalStore.RdbStore = await relationalStore.getRdbStore(context, config);
      await store.executeSql(CREATE_SQL);
      AlarmStore.store = store;
      AlarmStore.ready = true;
    } catch (err) {
      AlarmStore.ready = false;
    }
  }

  static isReady(): boolean {
    return AlarmStore.ready && AlarmStore.store !== null;
  }

  /** 把网关返回的列表写进本地（覆盖同 eventId） */
  static async saveMany(list: AlarmEvent[]): Promise<void> {
    if (!AlarmStore.isReady()) {
      return;
    }
    const store = AlarmStore.store as relationalStore.RdbStore;
    for (let i = 0; i < list.length; i++) {
      const a: AlarmEvent = list[i];
      const args: Array<string> = [
        a.eventId, a.ts, a.type, a.level, a.message, a.status,
        a.ackTs === null ? '' : a.ackTs,
        a.handleType === null ? '' : a.handleType,
        a.handleResult === null ? '' : a.handleResult,
        a.closedTs === null ? '' : a.closedTs,
        a.zone === null ? '' : a.zone,
        a.source, a.deviceId
      ];
      await store.executeSql(INSERT_SQL, args);
    }
  }

  /** 断网时从本地读；status 传 '' 表示全部 */
  static async query(status: string): Promise<AlarmEvent[]> {
    if (!AlarmStore.isReady()) {
      return [];
    }
    const store = AlarmStore.store as relationalStore.RdbStore;
    const sql: string = status.length > 0
      ? 'SELECT * FROM alarm WHERE status=? ORDER BY ts DESC LIMIT 50'
      : 'SELECT * FROM alarm ORDER BY ts DESC LIMIT 50';
    const args: Array<string> = status.length > 0 ? [status] : [];
    const result: AlarmEvent[] = [];
    const rs: relationalStore.ResultSet = await store.querySql(sql, args);
    try {
      while (rs.goToNextRow()) {
        const item: AlarmEvent = {
          eventId: rs.getString(rs.getColumnIndex('eventId')),
          deviceId: rs.getString(rs.getColumnIndex('deviceId')),
          ts: rs.getString(rs.getColumnIndex('ts')),
          type: rs.getString(rs.getColumnIndex('type')),
          level: rs.getString(rs.getColumnIndex('level')),
          message: rs.getString(rs.getColumnIndex('message')),
          status: rs.getString(rs.getColumnIndex('status')),
          ackTs: AlarmStore.nullable(rs.getString(rs.getColumnIndex('ackTs'))),
          ackOperator: null,
          handleType: AlarmStore.nullable(rs.getString(rs.getColumnIndex('handleType'))),
          handleResult: AlarmStore.nullable(rs.getString(rs.getColumnIndex('handleResult'))),
          closedTs: AlarmStore.nullable(rs.getString(rs.getColumnIndex('closedTs'))),
          zone: AlarmStore.nullable(rs.getString(rs.getColumnIndex('zone'))),
          source: rs.getString(rs.getColumnIndex('source')),
          escalated: false
        };
        result.push(item);
      }
    } finally {
      rs.close();
    }
    return result;
  }

  /** 本地回写（离线时先改本地，联网后以网关为准） */
  static async patchStatus(eventId: string, status: string, handleType: string,
                           handleResult: string, ts: string): Promise<void> {
    if (!AlarmStore.isReady()) {
      return;
    }
    const store = AlarmStore.store as relationalStore.RdbStore;
    await store.executeSql(
      'UPDATE alarm SET status=?, handleType=?, handleResult=?, closedTs=? WHERE eventId=?',
      [status, handleType, handleResult, ts, eventId]);
  }

  private static nullable(value: string): string | null {
    return value === null || value.length === 0 ? null : value;
  }
}
