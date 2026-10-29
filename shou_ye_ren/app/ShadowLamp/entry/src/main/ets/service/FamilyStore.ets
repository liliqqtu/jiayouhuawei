/**
 * 家庭组本地存储。
 *
 * 分工：家庭组本身由网关内存维护（重启即重置），APP 这边只保存“我属于哪个组、
 * 我叫什么、我是什么身份、网关地址”，这样重开 APP 不用重新加入。
 *
 * 为什么用 preferences 而不是数据库：数据量就 5 个字段、只读写单条，
 * preferences 足够且和「网关地址」同源，避免多一套存储。
 */
import { preferences } from '@kit.ArkData';
import { common } from '@kit.AbilityKit';
import { FamilyInfo } from '../model/Models';

const PREF_NAME: string = 'shadowlamp_family';
const KEY_FAMILY_ID: string = 'familyId';
const KEY_FAMILY_NAME: string = 'familyName';
const KEY_INVITE_CODE: string = 'inviteCode';
const KEY_MEMBER_NAME: string = 'memberName';
const KEY_ROLE: string = 'role';
const KEY_JOINED: string = 'joined';

export class FamilyStore {
  /** 是否已加入家庭组（决定启动进首页还是进“创建/加入”页） */
  static joined: boolean = false;
  static familyId: string = '';
  static familyName: string = '';
  static inviteCode: string = '';
  /** 我在这个组里的称呼，例如“大女儿”“网格员小李” */
  static memberName: string = '';
  /** owner / guardian */
  static role: string = '';

  private static pref: preferences.Preferences | null = null;
  private static ctx: common.UIAbilityContext | null = null;

  static async init(context: common.UIAbilityContext): Promise<void> {
    FamilyStore.ctx = context;
    try {
      FamilyStore.pref = await preferences.getPreferences(context, PREF_NAME);
      FamilyStore.joined = (await FamilyStore.pref.get(KEY_JOINED, false)) as boolean;
      FamilyStore.familyId = (await FamilyStore.pref.get(KEY_FAMILY_ID, '')) as string;
      FamilyStore.familyName = (await FamilyStore.pref.get(KEY_FAMILY_NAME, '')) as string;
      FamilyStore.inviteCode = (await FamilyStore.pref.get(KEY_INVITE_CODE, '')) as string;
      FamilyStore.memberName = (await FamilyStore.pref.get(KEY_MEMBER_NAME, '')) as string;
      FamilyStore.role = (await FamilyStore.pref.get(KEY_ROLE, '')) as string;
    } catch (err) {
      FamilyStore.joined = false;
    }
    // 家庭组名同步到 AppStorage，首页顶部和「我的」页用 @StorageLink 直接显示
    AppStorage.setOrCreate<string>('familyName', FamilyStore.familyName);
    AppStorage.setOrCreate<string>('familyRole', FamilyStore.role);
  }

  /** 加入/创建成功后落盘：把网关返回的信息 + 我的身份存下来 */
  static async save(info: FamilyInfo, myName: string): Promise<void> {
    FamilyStore.joined = true;
    FamilyStore.familyId = info.familyId;
    FamilyStore.familyName = info.familyName;
    FamilyStore.inviteCode = info.inviteCode;
    FamilyStore.memberName = myName;
    // 身份以网关成员表为准：名字对得上就是那条，否则按“监护人”处理
    FamilyStore.role = 'guardian';
    for (let i = 0; i < info.members.length; i++) {
      if (info.members[i].name === myName) {
        FamilyStore.role = info.members[i].role;
      }
    }
    AppStorage.setOrCreate<string>('familyName', info.familyName);
    AppStorage.setOrCreate<string>('familyRole', FamilyStore.role);

    await FamilyStore.ensurePref();
    if (FamilyStore.pref !== null) {
      await FamilyStore.pref.put(KEY_JOINED, true);
      await FamilyStore.pref.put(KEY_FAMILY_ID, info.familyId);
      await FamilyStore.pref.put(KEY_FAMILY_NAME, info.familyName);
      await FamilyStore.pref.put(KEY_INVITE_CODE, info.inviteCode);
      await FamilyStore.pref.put(KEY_MEMBER_NAME, myName);
      await FamilyStore.pref.put(KEY_ROLE, FamilyStore.role);
      await FamilyStore.pref.flush();
    }
  }

  /** 退出家庭组：只清本地关系，网关里的组不会散（别人还在用） */
  static async clear(): Promise<void> {
    FamilyStore.joined = false;
    FamilyStore.familyId = '';
    FamilyStore.familyName = '';
    FamilyStore.inviteCode = '';
    FamilyStore.memberName = '';
    FamilyStore.role = '';
    AppStorage.setOrCreate<string>('familyName', '');
    AppStorage.setOrCreate<string>('familyRole', '');
    await FamilyStore.ensurePref();
    if (FamilyStore.pref !== null) {
      await FamilyStore.pref.put(KEY_JOINED, false);
      await FamilyStore.pref.put(KEY_FAMILY_ID, '');
      await FamilyStore.pref.put(KEY_FAMILY_NAME, '');
      await FamilyStore.pref.put(KEY_INVITE_CODE, '');
      await FamilyStore.pref.put(KEY_MEMBER_NAME, '');
      await FamilyStore.pref.put(KEY_ROLE, '');
      await FamilyStore.pref.flush();
    }
  }

  /** 身份的中文名，直接给界面用 */
  static roleText(): string {
    return FamilyStore.role === 'owner' ? '户主' : (FamilyStore.role.length > 0 ? '监护人' : '未加入');
  }

  /** init 可能没跑完就被调用，这里兜底补一次 */
  private static async ensurePref(): Promise<void> {
    if (FamilyStore.pref === null && FamilyStore.ctx !== null) {
      try {
        FamilyStore.pref = await preferences.getPreferences(FamilyStore.ctx, PREF_NAME);
      } catch (err) {
        FamilyStore.pref = null;
      }
    }
  }
}
