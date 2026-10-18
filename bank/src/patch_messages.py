"""Build the ten localized resources used by the combined Bank patch.

构建合并 Bank 补丁使用的十套本地化资源。
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from tools import message_archive as message_codec
from tools.title_layout import build_title_archive
from title_font import build_title_fonts, character_widths
from title_messages import L_BUTTON, language_hint, mode_messages, mode_switch_hint, USER_LABEL_MESSAGES, USER_SWITCH_HINTS


ARCHIVES = (
    "0/0/4", "0/0/5", "0/0/6", "0/0/7", "0/0/8",
    "0/0/9", "0/1/0", "0/1/1", "0/1/2", "0/1/3",
)
MESSAGE_FILE_INDEX = 39
TICKET_INSUFFICIENT_FUNDS_LINE = 121
TITLE_LANGUAGE_LINE = 122
TITLE_SWITCH_LINE = 127
TITLE_USER_LABEL_LINE, TITLE_USER_HINT_LINE = 128, 129
MISMATCH_SOURCE_LINE, OFFLINE_MISMATCH_LINE = 10, 131
TITLE_SWITCH_MESSAGES = {archive: mode_switch_hint(archive) for archive in ARCHIVES}
TICKET_INSUFFICIENT_FUNDS_MESSAGES = {
    "0/0/4": "ざんだかが たりません。ついかしてから こうにゅうしてください。\nオフラインの かそうついかは、まいかい ねんかんりようけん１まいぶんの きんがくです。\nローカルせんようで、こうしきeショップの ざんだか・こうにゅうには かんけいしません。",
    "0/0/5": "残高が足りません。残高を追加してから購入してください。\nオフラインの仮想追加では、毎回年間利用券１枚分の金額が増えます。\n仮想追加と残高はローカル専用で、公式eショップの残高や購入とは無関係です。",
    "0/0/6": "Insufficient balance. Add credit before buying.\nOffline: each virtual top-up = annual pass price.\nLocal only; no link to eShop funds or purchases.",
    "0/0/7": "Solde insuffisant. Rechargez avant l’achat.\nHors ligne : chaque recharge fictive = prix du pass annuel.\nSimulation locale, sans lien avec solde/achats eShop.",
    "0/0/8": "Saldo insufficiente. Ricarica prima di acquistare.\nOffline: ogni ricarica virtuale = prezzo del pass annuo.\nSolo locale; senza legami con saldo/acquisti eShop.",
    "0/0/9": "Guthaben reicht nicht. Bitte vor dem Kauf aufladen.\nOffline: jede virtuelle Aufladung = Jahrespasspreis.\nNur lokal; unabhängig von eShop-Guthaben und Käufen.",
    "0/1/0": "Saldo insuficiente. Recarga antes de comprar.\nSin conexión: cada recarga virtual = precio del pase anual.\nSolo local; sin relación con saldo/compras de eShop.",
    "0/1/1": "잔액이 부족합니다. 충전 후 구매해 주세요.\n오프라인 가상 충전은 매번 연간 이용권 1장 가격만큼 증가합니다.\n가상 충전과 잔액은 로컬 모의용이며 공식 eShop 잔액·구매와 무관합니다.",
    "0/1/2": "余额不足，请充值后再购买。\n离线模式下为虚拟充值，每次充值增加1次年券所需金额。\n虚拟充值和余额仅为本地模拟使用，与官方eshop余额和购买行为无关。",
    "0/1/3": "餘額不足，請儲值後再購買。\n離線模式為虛擬儲值，每次增加1張年券所需金額。\n虛擬儲值和餘額僅供本機模擬，與官方eShop餘額及購買行為無關。",
}

NO_GAME_RECORD_LINE = 0
INTERNET_CONNECTION_LINE = 12
BANK_CONNECTION_LINE = 14
SAVE_LINE = 8
DISCONNECT_LINE = 13
MENU_LINE = 41
UNLOCK_CHALLENGE_SOURCE_LINE = 34

DOWNLOAD_PROGRESS_MESSAGES = {
    "0/0/4": "ポケモンバンクの データを\nローカルサーバーに ダウンロードしています……",
    "0/0/5": "ポケモンバンクのデータを\nローカルサーバーにダウンロードしています……",
    "0/0/6": "Downloading Pokémon Bank data\nto the local server...",
    "0/0/7": "Téléchargement des données de Banque Pokémon\nvers le serveur local…",
    "0/0/8": "Download dei dati della Banca Pokémon\nsul server locale...",
    "0/0/9": "Bankdaten werden auf den lokalen Server\nheruntergeladen...",
    "0/1/0": "Descargando los datos del Banco de Pokémon\nal servidor local…",
    "0/1/1": "포켓몬 뱅크 데이터를 로컬 서버로\n다운로드 중입니다…",
    "0/1/2": "正在将宝可梦虚拟银行数据\n下载到本地服务器……",
    "0/1/3": "正在將寶可夢虛擬銀行資料\n下載到本機伺服器……",
}

DOWNLOAD_MENU_MESSAGES = {
    "0/0/4": "ぎんこうデータを　ほんたいにダウンロード",
    "0/0/5": "銀行データを本体にダウンロード",
    "0/0/6": "Download Bank Data",
    "0/0/7": "Télécharger les données",
    "0/0/8": "Scarica dati Banca",
    "0/0/9": "Bankdaten laden",
    "0/1/0": "Descargar datos del Banco",
    "0/1/1": "뱅크 데이터 다운로드",
    "0/1/2": "下载银行数据到本地",
    "0/1/3": "下載銀行資料到本機",
}

DOWNLOAD_NO_GAME_MESSAGES = {
    "0/0/4": "つかえる ゲームの セーブデータが\nありません。サーバーから\nぎんこうデータを ダウンロードしますか？",
    "0/0/5": "利用できるゲームのセーブデータがありません。\nサーバーの銀行データをダウンロードしますか？",
    "0/0/6": "No usable game save was found.\nDownload Bank data from the server?",
    "0/0/7": "Aucune sauvegarde de jeu utilisable.\nTélécharger les données de Banque\ndepuis le serveur ?",
    "0/0/8": "Nessun salvataggio di gioco utilizzabile.\nScaricare i dati della Banca\ndal server?",
    "0/0/9": "Kein nutzbarer Spielstand gefunden.\nBankdaten vom Server herunterladen?",
    "0/1/0": "No hay ninguna partida utilizable.\n¿Descargar los datos del Banco\ndesde el servidor?",
    "0/1/1": "사용 가능한 게임 저장 데이터가 없습니다.\n서버에서 뱅크 데이터를\n다운로드하시겠습니까?",
    "0/1/2": "未找到可用的游戏存档。\n是否直接下载服务器银行数据？",
    "0/1/3": "未找到可用的遊戲存檔。\n是否直接下載伺服器銀行資料？",
}

DOWNLOAD_START_MESSAGES = {
    "0/0/4": "ダウンロードする",
    "0/0/5": "ダウンロード開始",
    "0/0/6": "Start Download",
    "0/0/7": "Télécharger",
    "0/0/8": "Avvia download",
    "0/0/9": "Download starten",
    "0/1/0": "Iniciar descarga",
    "0/1/1": "다운로드 시작",
    "0/1/2": "开始下载",
    "0/1/3": "開始下載",
}

DOWNLOAD_FAILED_MESSAGES = {
    "0/0/4": "ローカルサーバーに ぎんこうデータを\nほぞん できませんでした。\nせつぞくを きっています……",
    "0/0/5": "ローカルサーバーに銀行データを\n保存できませんでした。\nインターネットの接続を切っています……",
    "0/0/6": "Could not save Bank data to the local server.\nDisconnecting from the Internet...",
    "0/0/7": "Échec de la sauvegarde sur le serveur local.\nDéconnexion d’Internet…",
    "0/0/8": "Impossibile salvare i dati\nsul server locale.\nDisconnessione da Internet...",
    "0/0/9": "Bankdaten konnten nicht auf dem\nlokalen Server gespeichert werden.\nInternetverbindung wird getrennt...",
    "0/1/0": "No se pudieron guardar los datos\nen el servidor local.\nDesconectando de Internet…",
    "0/1/1": "로컬 서버에 뱅크 데이터를\n저장하지 못했습니다.\n인터넷 연결을 종료하는 중입니다…",
    "0/1/2": "未能将银行数据保存到本地服务器。\n正在断开互联网……",
    "0/1/3": "未能將銀行資料儲存到本機伺服器。\n正在斷開網際網路……",
}

UNLOCK_MENU_MESSAGES = {
    "0/0/4": "つよく アンロックする",
    "0/0/5": "強制アンロックモードへ",
    "0/0/6": "Enter Force Unlock",
    "0/0/7": "Déverrouillage forcé",
    "0/0/8": "Sblocco forzato",
    "0/0/9": "Zwangsentsperrung",
    "0/1/0": "Desbloqueo forzado",
    "0/1/1": "강제 잠금 해제 모드",
    "0/1/2": "进入强制解锁模式",
    "0/1/3": "進入強制解鎖模式",
}

# Native Mover wording (table 23, line 40), replacing only L with its button glyph.
# Both Japanese archives use the same kanji text. No extracted Mover ROM is needed.
# 沿用 Mover 原表 23 第 40 行，只替换 L 按键字形；两套日文写法相同，Bank 可独立编译。
TITLE_LANGUAGE_MESSAGES = {archive: language_hint(text) for archive, text in {
    "0/0/4": "Lボタンを押すと言語を選べます",
    "0/0/5": "Lボタンを押すと言語を選べます",
    "0/0/6": "Press the L Button to select a language",
    "0/0/7": "Appuyez sur le bouton L pour choisir la langue.",
    "0/0/8": "Premi il pulsante L per cambiare lingua.",
    "0/0/9": "Drücke die L-Taste, um die Sprache zu ändern.",
    "0/1/0": "Pulsa el botón L para elegir un idioma.",
    "0/1/1": "L 버튼을 누르면 언어를 선택할 수 있습니다",
    "0/1/2": "如果按L键，就可以选择语言。",
    "0/1/3": "按L鍵可選擇語言",
}.items()}

OFFLINE_INITIAL_CONNECT_MESSAGES = {
    "0/0/4": "せつぞくしています……",
    "0/0/5": "接続しています……",
    "0/0/6": "Connecting...",
    "0/0/7": "Connexion…",
    "0/0/8": "Connessione in corso...",
    "0/0/9": "Verbindung wird hergestellt...",
    "0/1/0": "Conectando...",
    "0/1/1": "연결 중입니다…",
    "0/1/2": "正在连接中……",
    "0/1/3": "正在連線中……",
}

OFFLINE_BANK_CONNECTION_MESSAGES = {
    "0/0/4": "ポケモンバンクの　ローカルサーバーと\nつうしん　しています",
    "0/0/5": "ポケモンバンクの　ローカルサーバーと\n通信しています",
    "0/0/6": "Communicating with the local\nPokémon Bank server...",
    "0/0/7": "Connexion au serveur local de Banque Pokémon…",
    "0/0/8": "Comunicazione col server locale\ndella Banca Pokémon in corso.",
    "0/0/9": "Datenübertragung über den lokalen Server\nvon Pokémon Bank läuft...",
    "0/1/0": "Conectando con el servidor local\ndel Banco de Pokémon...",
    "0/1/1": "포켓몬 뱅크의 로컬 서버와\n통신하고 있습니다",
    "0/1/2": "正在和宝可梦虚拟银行的\n本地服务器进行连接……",
    "0/1/3": "正在連線到\n寶可夢虛擬銀行的本機伺服器……",
}

OFFLINE_SAVE_MESSAGES = {
    "0/0/4": "レポートを　かいて\nローカルサーバーに　データを　そうしん　しています\nでんげんを　きらないで　ください",
    "0/0/5": "レポートを　書いて\nローカルサーバーに　データを　送信しています\n電源を　切らないで　ください",
    "0/0/6": "Saving the data and sending it\nto the local server...\nDon’t turn off the power.",
    "0/0/7": "Sauvegarde et envoi des données\nvers le serveur local…\nNe pas éteindre la console.",
    "0/0/8": "Salvataggio e invio dei dati\nal server locale in corso.\nNon spegnere la console.",
    "0/0/9": "Der Spielstand wird gespeichert und die Daten\nwerden auf den lokalen Server übertragen...\nBitte das System nicht ausschalten.",
    "0/1/0": "Guardando y enviando los datos\nal servidor local...\nNo apagues la consola.",
    "0/1/1": "리포트를 기록하고\n로컬 서버에 데이터를 송신하고 있습니다\n전원을 끄지 않도록 해주십시오",
    "0/1/2": "正在写入记录，\n并将数据发送到本地服务器。\n请勿切断电源。",
    "0/1/3": "正在寫入記錄，\n並將資料傳送到本機伺服器。\n請勿關閉電源。",
}

OFFLINE_DISCONNECT_MESSAGES = {
    "0/0/4": "せつだん　しています……",
    "0/0/5": "接続を切っています……",
    "0/0/6": "Disconnecting...",
    "0/0/7": "Déconnexion…",
    "0/0/8": "Disconnessione…",
    "0/0/9": "Verbindung wird getrennt...",
    "0/1/0": "Desconectando...",
    "0/1/1": "연결을 종료하는 중입니다…",
    "0/1/2": "正在断开连接……",
    "0/1/3": "正在中斷連線……",
}

SUPPORT_LINE = 43
MOVER_DOWNLOAD_LINE = 44
MOVER_INSTALLED_LINE = 45
HOME_LINE = 86
TITLE_HOME_LINE = 77
TITLE_VERSION_LINE = 78
MENU_MESSAGE_FILE_INDEX = 37
MENU_GREETING_LINE = 17
OFFLINE_MENU_GREETING_LINE = 27
DOWNLOAD_MENU_GREETING_LINE = 28
UNLOCK_MENU_GREETING_LINE = 29
BLANK_LINE = 96
DOWNLOAD_PROGRESS_LINE = 97
DOWNLOAD_SUCCESS_LINE = 98
DOWNLOAD_USE_BANK_LINE = 99
OFFLINE_INITIAL_CONNECT_LINE = 100
OFFLINE_BANK_CONNECTION_LINE = 101
OFFLINE_SAVE_LINE = 102
OFFLINE_DISCONNECT_LINE = 103
TITLE_MODE_OFFLINE_LINE = 104
TITLE_MODE_DOWNLOAD_LINE = 105
# Retired standalone disabled label. Preserve its empty slot and all later IDs.
# 原单独“已禁用”文本撤除；保留空位及后续编号。
RESERVED_DISABLED_LINE = 106
# Retired in-mode language entry; retain an empty slot so later IDs are stable.
# 模式内语言入口已撤除；保留空位，不改变后续消息编号。
RESERVED_LANGUAGE_MENU_LINE = 107
TITLE_MODE_UNLOCK_LINE = 108
UNLOCK_USE_BANK_LINE = 109
UNLOCK_GAME_SELECTION_LINE = 110
UNLOCK_CHALLENGE_LINE = 111
NO_GAME_BLOCKED_LINE = 112
UNLOCK_SUPPORT_REFERENCE_LINE = 113
TITLE_MODE_ORIGINAL_LINE = 114
DOWNLOAD_NO_GAME_LINE = 115
DOWNLOAD_FAILED_LINE = 116
DOWNLOAD_START_LINE = 117
UNLOCK_NO_RECOVERY_LINE = 118
UNLOCK_RECOVERED_LINE = 119
UNLOCK_FORCED_RECOVERED_LINE = 120
TITLE_TEXT_BUFFER_LENGTH = 54
TITLE_MODE_BUFFER_LENGTH = 127
MENU_TEXT_BUFFER_LENGTH = 54
RESERVED_SUPPORT_DISABLED_LINE = 123
INVALID_SUPPORT_LINE = 130
DISABLED_MENU_LINES = {
    MOVER_DOWNLOAD_LINE: 124,
    MOVER_INSTALLED_LINE: 125,
    HOME_LINE: 126,
}

DISABLED_MESSAGES = {
    "0/0/4": "つかえません",
    "0/0/5": "使用できません",
    "0/0/6": "Disabled",
    "0/0/7": "Indisponible",
    "0/0/8": "Disattivato",
    "0/0/9": "Deaktiviert",
    "0/1/0": "Desactivado",
    "0/1/1": "사용할 수 없습니다",
    "0/1/2": "已禁用",
    "0/1/3": "已停用",
}

INVALID_SUPPORT_SUFFIXES = {
    "0/0/4": "（むこう）", "0/0/5": "（無効）",
    "0/0/6": " (Invalid)", "0/0/7": " (Invalide)",
    "0/0/8": " (Non valido)", "0/0/9": " (Ungültig)",
    "0/1/0": " (No válido)", "0/1/1": " (유효하지 않음)",
    "0/1/2": "（无效）", "0/1/3": "（無效）",
}

def build_invalid_support_label(archive: str, original_lines: list[str]) -> str:
    text = original_lines[43] + INVALID_SUPPORT_SUFFIXES[archive]
    if len(text_code_units(text)) > MENU_TEXT_BUFFER_LENGTH or "\n" in text:
        raise ValueError(f"invalid support menu label exceeds the pane capacity: {archive}")
    return text


def build_disabled_menu_labels(archive: str, original_lines: list[str]) -> dict[int, str]:
    """Keep the full native label and append a localized disabled suffix.

    保留完整原条目，仅在末尾加本地化禁用后缀；越界直接阻止构建，不截断文字。
    """
    if archive in ("0/0/4", "0/0/5", "0/1/2", "0/1/3"):
        suffix = f"（{DISABLED_MESSAGES[archive]}）"
    else:
        suffix = f" ({DISABLED_MESSAGES[archive]})"
    result = {target: original_lines[source] + suffix
              for source, target in DISABLED_MENU_LINES.items()}
    for line, text in result.items():
        if len(text_code_units(text)) > MENU_TEXT_BUFFER_LENGTH:
            raise ValueError(f"disabled menu label exceeds the pane capacity: {archive}, line {line}")
    return result

# These stock greetings occupy multiple lines in the listed archives. Keep
# their original meaning in a one-line form so the mode explanation remains
# visible without requiring an extra page advance.
# 下列语言的原版欢迎语会占用多行。将其含义保留为一行，使后续模式说明无需翻页
# 即可显示。
SHORT_MENU_GREETINGS = {
    "0/0/4": "ポケモンバンクへ ようこそ！",
    "0/0/5": "ポケモンバンクへようこそ！",
    "0/0/6": "Welcome to Pokémon Bank!",
    "0/1/1": "포켓몬 뱅크에 어서 와!",
}

# Use the same private-use R-button glyph in every localized title hint.
# 所有语言的标题提示统一使用同一个专用区 R 键字形。
R_BUTTONS = {
    "0/0/4": "\ue005",
    "0/0/5": "\ue005",
    "0/0/6": "\ue005",
    "0/0/7": "\ue005",
    "0/0/8": "\ue005",
    "0/0/9": "\ue005",
    "0/1/0": "\ue005",
    "0/1/1": "\ue005",
    "0/1/2": "\ue005",
    "0/1/3": "\ue005",
}

# The shared 3DS font provides native L and A glyphs, but no native 3DS
# START glyph. U+E045 is the Wii Remote "+" button, so START stays as text.
# 3DS 共享字体提供原生 L、A 键字形，但没有原生的 3DS START 字形。
# U+E045 实际显示为 Wii Remote 的“+”键，因此 START 继续使用普通文本。
A_BUTTON = "\ue000"

# One additional page after the stock mismatch notice, or the complete Unlock
# game-selection notice. Keep the risk, all-save condition and hold gesture.
# 原版不匹配提示后仅新增一页；解锁选游戏提示同样说明风险、全档核对及按住组合键。
def force_unlock_messages(*, local: bool) -> dict[str, str]:
    server = {
        "0/0/4": "ローカルサーバー" if local else "こうしきサーバー",
        "0/0/5": "ローカルサーバー" if local else "公式サーバー",
        "0/0/6": "local server" if local else "official server",
        "0/0/7": "serveur local" if local else "serveur officiel",
        "0/0/8": "server locale" if local else "server ufficiale",
        "0/0/9": "lokalen Server" if local else "offiziellen Server",
        "0/1/0": "servidor local" if local else "servidor oficial",
        "0/1/1": "로컬 서버" if local else "공식 서버",
        "0/1/2": "本地服务器" if local else "官方服务器",
        "0/1/3": "本機伺服器" if local else "官方伺服器",
    }
    keys = f"{L_BUTTON} + {A_BUTTON} + START"
    return {
        "0/0/4": ("かいじょで まえにおくった ポケモンがきえるかも。\n"
                  f"どのセーブも {server['0/0/4']}と あわないときだけ\n"
                  f"ゲームのけっていじ {keys}を おしつづける"),
        "0/0/5": ("強制解除で前回の転送ポケモンを失う恐れ。\n"
                  f"全セーブが{server['0/0/5']}と不一致と確認後、\n"
                  f"ゲームを選び、{keys}を押したまま決定。"),
        "0/0/6": ("Forced unlock may lose your last Bank transfer.\n"
                  f"If no save matches the {server['0/0/6']}, pick a game.\n"
                  f"At confirmation hold {keys} to force unlock."),
        "0/0/7": ("Forcer le déblocage peut perdre le dernier transfert.\n"
                  f"Si aucune sauvegarde ne correspond au {server['0/0/7']},\n"
                  f"choisissez un jeu ; validez en maintenant {keys}."),
        "0/0/8": ("Sblocco forzato: puoi perdere l’ultimo trasferimento.\n"
                  f"Se nessun salvataggio coincide col {server['0/0/8']},\n"
                  f"scegli un gioco; conferma tenendo premuti {keys}."),
        "0/0/9": ("Zwangsentsperren kann den letzten Transfer löschen.\n"
                  f"Passt kein Spielstand zum {server['0/0/9']}: Spiel wählen.\n"
                  f"Beim Bestätigen {keys} gedrückt halten."),
        "0/1/0": ("Forzar el desbloqueo puede perder tu último traslado.\n"
                  f"Si ninguna partida coincide con el {server['0/1/0']},\n"
                  f"elige un juego; al confirmar, mantén {keys}."),
        "0/1/1": ("강제 해제 시 지난 전송 포켓몬을 잃을 수 있습니다.\n"
                  f"모든 저장이 {server['0/1/1']}와 불일치할 때만 게임 선택 후,\n"
                  f"확인 시 {keys}를 누르고 계세요."),
        "0/1/2": ("强制解锁可能使上次传输的宝可梦丢失。\n"
                  f"确认所有存档均无法匹配{server['0/1/2']}后，选游戏；\n"
                  f"确认时按住{keys}进入强制解锁界面。"),
        "0/1/3": ("強制解鎖可能使上次傳送的寶可夢遺失。\n"
                  f"確認所有存檔均無法與{server['0/1/3']}相符後，選遊戲；\n"
                  f"確認時按住{keys}進入強制解鎖畫面。"),
    }

OFFLINE_MISMATCH_MESSAGES = force_unlock_messages(local=True)

# Four appended BMG entries bind only the independent mode pane. Native HOME
# and version messages remain unchanged; each R press rebinds only mode.
# 四条追加文本仅绑定独立模式窗格；HOME 和版本号原文保留，R 只更新模式窗格。
TITLE_MODE_OFFLINE_MESSAGES = mode_messages("offline")
TITLE_MODE_DOWNLOAD_MESSAGES = mode_messages("download")
TITLE_MODE_UNLOCK_MESSAGES = mode_messages("unlock")
TITLE_MODE_ORIGINAL_MESSAGES = mode_messages("original")

OFFLINE_MENU_GREETINGS = {
    "0/0/4": "げんざいのモード：オフラインモード\nローカルと こうしきサーバーの データは べつです",
    "0/0/5": "現在のモード：オフラインモード\nローカルと公式サーバーのデータは別です",
    "0/0/6": "Current Mode: Offline Mode\nLocal and official server data are separate.",
    "0/0/7": "Mode actuel : Mode hors ligne\nDonnées séparées entre serveurs local et officiel.",
    "0/0/8": "Modalità attuale: Offline\nDati separati tra server locale e ufficiale.",
    "0/0/9": "Aktueller Modus: Offline\nLokale und offizielle Serverdaten sind getrennt.",
    "0/1/0": "Modo actual: Sin conexión\nDatos independientes en servidores local y oficial.",
    "0/1/1": "현재 모드: 오프라인 모드\n로컬 서버와 공식 서버의 데이터는 별개입니다.",
    "0/1/2": "当前模式：离线模式\n本地服务器与官方服务器数据彼此独立。",
    "0/1/3": "目前模式：離線模式\n本機伺服器與官方伺服器資料彼此獨立。",
}

DOWNLOAD_MENU_GREETINGS = {
    "0/0/4": "げんざいのモード：ダウンロードモード\nローカルサーバーへ ダウンロードして うわがきします",
    "0/0/5": "現在のモード：ダウンロードモード\nローカルサーバーへダウンロードして上書きします",
    "0/0/6": "Current Mode: Download Mode\nDownload server data to replace local server data.",
    "0/0/7": "Mode actuel : Téléchargement\nTélécharge et remplace les données du serveur local.",
    "0/0/8": "Modalità attuale: Download\nScarica e sovrascrive i dati del server locale.",
    "0/0/9": "Aktueller Modus: Download\nServerdaten laden und lokale Serverdaten ersetzen.",
    "0/1/0": "Modo actual: Descarga\nDescarga y reemplaza los datos del servidor local.",
    "0/1/1": "현재 모드: 다운로드 모드\n서버 데이터를 로컬 서버에 다운로드하여 덮어씁니다.",
    "0/1/2": "当前模式：下载模式\n将服务器银行数据下载到本地服务器并覆盖现有数据。",
    "0/1/3": "目前模式：下載模式\n將伺服器銀行資料下載到本機伺服器並覆蓋現有資料。",
}

UNLOCK_MENU_GREETINGS = {
    "0/0/4": "げんざいのモード：アンロックモード\nセーブが あわない ときに つよく アンロックします",
    "0/0/5": "現在のモード：アンロックモード\nセーブデータ不一致を強制解除します。",
    "0/0/6": "Current Mode: Unlock Mode\nForce-unlocks mismatched save data.",
    "0/0/7": "Mode actuel : Déverrouillage\nDéverrouille si les sauvegardes diffèrent.",
    "0/0/8": "Modalità attuale: Sblocco\nSblocca se i salvataggi non coincidono.",
    "0/0/9": "Aktueller Modus: Entsperren\nEntsperrt bei unpassendem Spielstand.",
    "0/1/0": "Modo actual: Desbloqueo\nDesbloquea si no coincide la partida.",
    "0/1/1": "현재 모드: 잠금 해제 모드\n저장 데이터 불일치를 강제로 해제할 때 사용합니다.",
    "0/1/2": "当前模式：解锁模式\n用于强制解锁存档不匹配的情况。",
    "0/1/3": "目前模式：解鎖模式\n用於強制解鎖存檔不相符的情況。",
}

DOWNLOAD_SUCCESS_MESSAGES = {
    "0/0/4": "ぎんこうデータを ローカルサーバーに\nダウンロードしました\nタイトルにもどります……",
    "0/0/5": "銀行データをローカルサーバーに\nダウンロードしました\nタイトルに戻ります……",
    "0/0/6": "Bank data was downloaded to the local server.\nReturning to the title screen...",
    "0/0/7": "Les données de Banque ont été téléchargées\nsur le serveur local. Retour à l’écran-titre…",
    "0/0/8": "I dati della Banca sono stati scaricati\nsul server locale. Ritorno al titolo...",
    "0/0/9": "Bankdaten wurden auf den lokalen Server\ngeladen. Rückkehr zum Titelbildschirm...",
    "0/1/0": "Los datos del Banco se descargaron\nen el servidor local. Volviendo al título...",
    "0/1/1": "뱅크 데이터를 로컬 서버에\n다운로드했습니다. 타이틀 화면으로 돌아갑니다…",
    "0/1/2": "已将银行数据下载到本地服务器。\n正在返回标题界面……",
    "0/1/3": "已將銀行資料下載到本機伺服器。\n正在返回標題畫面……",
}

UNLOCK_GAME_SELECTION_MESSAGES = force_unlock_messages(local=False)

UNLOCK_NO_RECOVERY_MESSAGES = {
    "0/0/4": "こうしきサーバーは せいじょうです。\nアンロックモードは ふようです。",
    "0/0/5": "公式サーバーは正常です。\nアンロックモードは不要です。",
    "0/0/6": "Official server status is normal.\nUnlock Mode is not needed.",
    "0/0/7": "Le serveur officiel fonctionne normalement.\nLe déverrouillage n'est pas nécessaire.",
    "0/0/8": "Lo stato del server ufficiale è normale.\nNon è necessario sbloccare.",
    "0/0/9": "Der offizielle Serverstatus ist normal.\nEntsperren ist nicht erforderlich.",
    "0/1/0": "El estado del servidor oficial es normal.\nNo se necesita desbloquear.",
    "0/1/1": "공식 서버 상태가 정상입니다.\n잠금 해제 모드에 들어갈 필요가 없습니다.",
    "0/1/2": "官方服务器状态正常，\n无需进入解锁模式。",
    "0/1/3": "官方伺服器狀態正常，\n無需進入解鎖模式。",
}

UNLOCK_RECOVERED_MESSAGES = {
    "0/0/4": "じどうふっきゅうが おわりました。\nこうしきサーバーの ロックを かいじょしました。",
    "0/0/5": "自動復旧が完了しました。\n公式サーバーのロックを解除しました。",
    "0/0/6": "Automatic recovery completed.\nThe official server lock has been cleared.",
    "0/0/7": "Récupération automatique terminée.\nLe verrou du serveur officiel est levé.",
    "0/0/8": "Ripristino automatico completato.\nIl blocco del server ufficiale è rimosso.",
    "0/0/9": "Automatische Wiederherstellung beendet.\nDie Sperre des offiziellen Servers ist aufgehoben.",
    "0/1/0": "Recuperación automática completada.\nEl servidor oficial se ha desbloqueado.",
    "0/1/1": "자동 복구가 완료되었습니다.\n공식 서버 잠금이 해제되었습니다.",
    "0/1/2": "已自动完成事务恢复。\n官方服务器锁定已解除。",
    "0/1/3": "已自動完成交易復原。\n官方伺服器鎖定已解除。",
}

UNLOCK_FORCED_RECOVERED_MESSAGES = {
    "0/0/4": "きょうせいアンロックが おわりました。\nこうしきサーバーの ロックを かいじょしました。",
    "0/0/5": "強制アンロックが完了しました。\n公式サーバーのロックを解除しました。",
    "0/0/6": "Forced unlock completed.\nThe official server lock has been cleared.",
    "0/0/7": "Déverrouillage forcé terminé.\nLe verrou du serveur officiel est levé.",
    "0/0/8": "Sblocco forzato completato.\nIl blocco del server ufficiale è rimosso.",
    "0/0/9": "Zwangsentsperren abgeschlossen.\nDie Sperre des offiziellen Servers ist aufgehoben.",
    "0/1/0": "Desbloqueo forzado completado.\nEl servidor oficial se ha desbloqueado.",
    "0/1/1": "강제 잠금 해제가 완료되었습니다.\n공식 서버 잠금이 해제되었습니다.",
    "0/1/2": "强制解锁已完成。\n官方服务器锁定已解除。",
    "0/1/3": "強制解鎖已完成。\n官方伺服器鎖定已解除。",
}

UNLOCK_SUPPORT_REFERENCE_LABELS = {
    "0/0/4": "サポートに つたえる ばんごう：",
    "0/0/5": "サポートに伝える番号：",
    "0/0/6": "Support reference number:",
    "0/0/7": "Numéro pour l’assistance :",
    "0/0/8": "Numero per l'assistenza:",
    "0/0/9": "Nummer für den Support:",
    "0/1/0": "Número para soporte:",
    "0/1/1": "고객 지원용 번호:",
    "0/1/2": "提供给客服的编号：",
    "0/1/3": "提供給客服的編號：",
}

UNLOCK_CODE_LABELS = {
    "0/0/4": "アンロックコードを にゅうりょく：",
    "0/0/5": "解除コードを入力：",
    "0/0/6": "Enter unlock code: ",
    "0/0/7": "Entrez le code de déblocage : ",
    "0/0/8": "Inserisci il codice di sblocco: ",
    "0/0/9": "Entsperrcode eingeben: ",
    "0/1/0": "Introduce el código de desbloqueo: ",
    "0/1/1": "잠금 해제 코드 입력: ",
    "0/1/2": "请输入解锁码：",
    "0/1/3": "請輸入解鎖碼：",
}


def unlock_number_tag(source_values: list[int]) -> list[int]:
    """Extract the stock challenge-number expansion tag.

    提取原版挑战编号的数字展开标签。
    """
    tag_prefix = (0x0010,0x0003,0x0207)
    tag_index = next(
        (
            index
            for index in range(len(source_values)-4)
            if tuple(source_values[index : index + 3]) == tag_prefix
        ),
        -1,
    )
    if tag_index < 0:
        raise ValueError("challenge message has no number-register tag")
    return list(source_values[tag_index : tag_index + 5])


def text_code_units(text: str) -> list[int]:
    """Encode text as unterminated UTF-16 code units.

    将文本编码为不带终止符的 UTF-16 代码单元。
    """
    encoded = text.encode("utf-16le")
    return [
        int.from_bytes(encoded[index : index + 2], "little")
        for index in range(0, len(encoded), 2)
    ]


def finish_message_values(values: list[int]) -> list[int]:
    """Append the terminator and preserve the message-file alignment.

    追加终止符并保持消息文件所需的对齐。
    """
    values = list(values)
    values.append(0)
    if len(values) & 1:
        values.append(0)
    return values


def build_offline_mismatch_values(source_values: list[int], notice: str) -> list[int]:
    """Preserve stock trainer/number tags, then append exactly one native page.

    保留原版姓名／编号展开标签（参数可为零），仅追加一页原生翻页说明。
    """
    position = 0
    while position < len(source_values):
        unit = source_values[position]
        if not unit:
            break
        if unit == 0x0010:
            if position + 2 >= len(source_values):
                raise ValueError("truncated mismatch message tag")
            count = source_values[position + 1]
            if not count or position + 2 + count > len(source_values):
                raise ValueError("invalid mismatch message tag length")
            position += count + 2
        else:
            position += 1
    if position == len(source_values):
        raise ValueError("unterminated stock mismatch message")
    values = source_values[:position] + [0x0010, 0x0001, 0xBE01, 0x000A]
    values.extend(text_code_units(notice))
    return finish_message_values(values)


def build_no_game_prompt_values(source_values: list[int]) -> list[int]:
    """Keep both stock no-game pages and their confirmation waits.

    保留原版无游戏提示的前两页及按键等待，去掉最后的 HOME 询问。
    """
    page_break = [0x0010, 0x0001, 0xBE01]
    page_ends = [
        index
        for index in range(len(source_values) - 2)
        if source_values[index : index + 3] == page_break
    ]
    if len(page_ends) != 2:
        raise ValueError("no-game message must contain two stock page breaks")
    return finish_message_values(source_values[:page_ends[1] + len(page_break)])


def build_support_reference_values(source_values: list[int], label: str) -> list[int]:
    """Build the corrected two-line support-reference prompt.

    构建修正后的两行客服参考编号提示。
    """
    values = text_code_units(label)
    values.append(0x000A)
    values.extend(unlock_number_tag(source_values))
    return finish_message_values(values)


def build_unlock_prompt_values(
    source_values: list[int], support_label: str, unlock_label: str
) -> list[int]:
    """Build the corrected prompt and append number register 1.

    构建修正后的提示，并追加展开数字寄存器 1 的第三行。
    """
    # Keep both parameters of the stock challenge-number tag except for the
    # register ID itself. Some language archives use a nonzero second parameter.
    # 除寄存器编号外保留原挑战码标签的两个参数；部分语言档案的第二参数不为零。
    challenge_tag = unlock_number_tag(source_values)
    unlock_tag = list(challenge_tag)
    unlock_tag[3] = 1
    values = text_code_units(support_label)
    values.append(0x000A)
    values.extend(challenge_tag)
    values.append(0x000A)
    values.extend(text_code_units(unlock_label))
    values.extend(unlock_tag)
    return finish_message_values(values)


def verify_localized_server_messages() -> None:
    """Validate every language and the three-row dialog budget.

    检查全部语言是否齐全，以及相关对话是否符合三行限制。
    """
    tables=(TICKET_INSUFFICIENT_FUNDS_MESSAGES,DOWNLOAD_PROGRESS_MESSAGES,
            DOWNLOAD_FAILED_MESSAGES,OFFLINE_BANK_CONNECTION_MESSAGES,OFFLINE_SAVE_MESSAGES,
            OFFLINE_MENU_GREETINGS,DOWNLOAD_MENU_GREETINGS,DOWNLOAD_SUCCESS_MESSAGES,
            UNLOCK_GAME_SELECTION_MESSAGES,OFFLINE_MISMATCH_MESSAGES,UNLOCK_NO_RECOVERY_MESSAGES,
            UNLOCK_RECOVERED_MESSAGES,UNLOCK_FORCED_RECOVERED_MESSAGES)
    for table in tables:
        if set(table)!=set(ARCHIVES):
            raise ValueError("server message translations do not cover every language")
        for archive,text in table.items():
            rows=len(text.splitlines())
            if not 1<=rows<=3:
                raise ValueError(f"server dialog exceeds three rows in {archive}")
            if table is TICKET_INSUFFICIENT_FUNDS_MESSAGES and rows!=3:
                raise ValueError(f"virtual-credit notice must have three rows in {archive}")
            if (table is OFFLINE_MENU_GREETINGS or table is DOWNLOAD_MENU_GREETINGS) and rows!=2:
                raise ValueError(f"mode description must leave one row for the native greeting in {archive}")


def verify_force_unlock_layout(source_romfs: Path) -> None:
    """Keep each warning inside the native three 360px, 66-unit text panes.

    每页严格三行，按原字库实际字宽核对 360 像素和 66 单元窗格，不缩小字号。
    """
    fonts = message_codec.read_garc((source_romfs / "a/0/2/3").read_bytes())[2]
    widths = [character_widths(fonts[index].files[0]) for index in range(3)]
    for table in (OFFLINE_MISMATCH_MESSAGES, UNLOCK_GAME_SELECTION_MESSAGES):
        for archive, text in table.items():
            rows = text.splitlines()
            if len(rows) != 3:
                raise ValueError(f"force-unlock notice must have exactly three rows: {archive}")
            advances = widths[1 if archive == "0/1/2" else 2 if archive == "0/1/3" else 0]
            for row in rows:
                if any(char not in advances for char in row):
                    raise ValueError(f"force-unlock notice has a missing glyph: {archive} {row!r}")
                if len(text_code_units(row)) >= 66 or sum(advances[char] for char in row) > 360:
                    raise ValueError(f"force-unlock notice overflows a native row: {archive} {row!r}")
            for button in (L_BUTTON, A_BUTTON, "START"):
                if text.count(button) != 1:
                    raise ValueError(f"force-unlock notice has invalid button text: {archive}")


def verify_kana_archive_messages() -> None:
    """Reject kanji in every custom string emitted for the kana-only archive.

    拒绝写入假名专用文本库的任何自定义汉字。
    """
    archive = "0/0/4"
    # Native HOME and the copied Mover title hint already contain kanji in this
    # archive. Keep those native conventions; other custom messages stay kana-only.
    # 原版 HOME 及沿用的 Mover 标题提示本身含汉字，保留原样；其余自定义文本用假名。
    texts = (
        TICKET_INSUFFICIENT_FUNDS_MESSAGES[archive],
        DISABLED_MESSAGES[archive],
        OFFLINE_INITIAL_CONNECT_MESSAGES[archive],
        OFFLINE_BANK_CONNECTION_MESSAGES[archive],
        OFFLINE_SAVE_MESSAGES[archive],
        OFFLINE_DISCONNECT_MESSAGES[archive],
        OFFLINE_MENU_GREETINGS[archive],
        DOWNLOAD_MENU_GREETINGS[archive],
        DOWNLOAD_SUCCESS_MESSAGES[archive],
        DOWNLOAD_PROGRESS_MESSAGES[archive],
        DOWNLOAD_MENU_MESSAGES[archive],
        DOWNLOAD_NO_GAME_MESSAGES[archive],
        DOWNLOAD_START_MESSAGES[archive],
        DOWNLOAD_FAILED_MESSAGES[archive],
        UNLOCK_MENU_MESSAGES[archive],
        UNLOCK_MENU_GREETINGS[archive],
        UNLOCK_GAME_SELECTION_MESSAGES[archive],
        OFFLINE_MISMATCH_MESSAGES[archive],
        UNLOCK_NO_RECOVERY_MESSAGES[archive],
        UNLOCK_RECOVERED_MESSAGES[archive],
        UNLOCK_FORCED_RECOVERED_MESSAGES[archive],
        UNLOCK_SUPPORT_REFERENCE_LABELS[archive],
        UNLOCK_CODE_LABELS[archive],
    )
    for text in texts:
        kanji = [char for char in text if "\u3400" <= char <= "\u9fff" or "\uf900" <= char <= "\ufaff"]
        if kanji:
            raise ValueError(f"kana archive contains kanji: {''.join(kanji)} in {text!r}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-romfs", required=True, type=Path)
    parser.add_argument("--output-romfs", required=True, type=Path)
    parser.add_argument(
        "--title-r-glyph-test",
        action="store_true",
        help="append only the private-use R glyph to the stock HOME line",
    )
    args = parser.parse_args()
    subprocess.run([sys.executable, "-B", str(Path(__file__).absolute().parents[2] /
                    "common/tools/check_file_links.py"), "--recursive", "--allow-missing",
                    *(str(path) for path in vars(args).values() if isinstance(path, Path))], check=True)
    verify_localized_server_messages()
    verify_kana_archive_messages()
    verify_force_unlock_layout(args.source_romfs)
    build_title_archive(args.source_romfs, args.output_romfs)
    build_title_fonts(args.source_romfs, args.output_romfs)

    for archive in ARCHIVES:
        source = args.source_romfs / "a" / Path(archive)
        destination = args.output_romfs / "a" / Path(archive)
        version, alignment, entries = message_codec.read_garc(source.read_bytes())
        entry = entries[MESSAGE_FILE_INDEX]
        original = entry.files[0]
        original_lines = message_codec.read_message_lines(original)
        section_offset = message_codec.u32(original, 12)
        disconnect_flags = message_codec.u16(
            original, section_offset + 4 + DISCONNECT_LINE * 8 + 6
        )
        bank_flags = message_codec.u16(
            original, section_offset + 4 + BANK_CONNECTION_LINE * 8 + 6
        )
        use_bank_flags = message_codec.u16(
            original, section_offset + 4 + MENU_LINE * 8 + 6
        )
        game_selection_flags = message_codec.u16(
            original, section_offset + 4 + 3 * 8 + 6
        )
        title_mode_flags = message_codec.u16(
            original, section_offset + 4 + TITLE_HOME_LINE * 8 + 6
        )
        no_game_flags = message_codec.u16(
            original, section_offset + 4 + NO_GAME_RECORD_LINE * 8 + 6
        )
        download_start_flags = message_codec.u16(
            original, section_offset + 4 + 0x59 * 8 + 6
        )
        internet_flags = message_codec.u16(
            original, section_offset + 4 + INTERNET_CONNECTION_LINE * 8 + 6
        )
        save_flags = message_codec.u16(
            original, section_offset + 4 + SAVE_LINE * 8 + 6
        )
        unlock_challenge_flags = message_codec.u16(
            original,
            section_offset + 4 + UNLOCK_CHALLENGE_SOURCE_LINE * 8 + 6,
        )
        unlock_challenge_source = message_codec.read_message_line_values(
            original, UNLOCK_CHALLENGE_SOURCE_LINE
        )
        offline_mismatch = build_offline_mismatch_values(
            message_codec.read_message_line_values(original, MISMATCH_SOURCE_LINE),
            OFFLINE_MISMATCH_MESSAGES[archive],
        )
        mismatch_flags = message_codec.u16(
            original, section_offset + 4 + MISMATCH_SOURCE_LINE * 8 + 6
        )
        unlock_support_reference = build_support_reference_values(
            unlock_challenge_source, UNLOCK_SUPPORT_REFERENCE_LABELS[archive]
        )
        unlock_challenge = build_unlock_prompt_values(
            unlock_challenge_source,
            UNLOCK_SUPPORT_REFERENCE_LABELS[archive],
            UNLOCK_CODE_LABELS[archive],
        )
        no_game_prompt = build_no_game_prompt_values(
            message_codec.read_message_line_values(original, NO_GAME_RECORD_LINE)
        )
        title_language = TITLE_LANGUAGE_MESSAGES[archive]
        disabled_labels = build_disabled_menu_labels(archive, original_lines)
        if len(text_code_units(title_language)) > TITLE_TEXT_BUFFER_LENGTH:
            raise ValueError(f"title language hint exceeds the pane capacity: {archive}")
        if args.title_r_glyph_test:
            # Diagnostic archive: isolate private-use glyph rendering from
            # multiline layout and localized text length.
            # 诊断档案：将专用区字形渲染与多行布局、本地化文本长度完全分离。
            title_mode_offline = R_BUTTONS[archive]
            title_mode_download = title_mode_offline
            title_mode_unlock = title_mode_offline
            title_mode_original = title_mode_offline
        else:
            title_mode_offline = TITLE_MODE_OFFLINE_MESSAGES[archive]
            title_mode_download = TITLE_MODE_DOWNLOAD_MESSAGES[archive]
            title_mode_unlock = TITLE_MODE_UNLOCK_MESSAGES[archive]
            title_mode_original = TITLE_MODE_ORIGINAL_MESSAGES[archive]
        # The independent mode pane reserves 127 UTF-16 units. Native HOME
        # keeps its original 54-unit pane and unmodified message.
        # 独立模式窗格预留 127 个 UTF-16 单元；HOME 保留原 54 单元窗格和完整原文。
        for mode_name, title_text in (
            ("offline", title_mode_offline),
            ("download", title_mode_download),
            ("unlock", title_mode_unlock),
            ("original", title_mode_original),
        ):
            if len(text_code_units(title_text)) > TITLE_MODE_BUFFER_LENGTH or len(title_text.splitlines()) != 1:
                raise ValueError(
                    f"{archive} {mode_name} title text exceeds "
                    f"{TITLE_MODE_BUFFER_LENGTH} UTF-16 units: {len(text_code_units(title_text))}"
                )
        entry.files[0] = message_codec.patch_message_file(
            original,
            {},
            (
                ("", disconnect_flags),
                (DOWNLOAD_PROGRESS_MESSAGES[archive], bank_flags),
                (DOWNLOAD_SUCCESS_MESSAGES[archive], disconnect_flags),
                (DOWNLOAD_MENU_MESSAGES[archive], use_bank_flags),
                (OFFLINE_INITIAL_CONNECT_MESSAGES[archive], internet_flags),
                (OFFLINE_BANK_CONNECTION_MESSAGES[archive], bank_flags),
                (OFFLINE_SAVE_MESSAGES[archive], save_flags),
                (OFFLINE_DISCONNECT_MESSAGES[archive], disconnect_flags),
                (title_mode_offline, title_mode_flags),
                (title_mode_download, title_mode_flags),
                ("", use_bank_flags),
                ("", use_bank_flags),
                (title_mode_unlock, title_mode_flags),
                (UNLOCK_MENU_MESSAGES[archive], use_bank_flags),
                (UNLOCK_GAME_SELECTION_MESSAGES[archive], game_selection_flags),
            ),
            (
                (unlock_challenge, unlock_challenge_flags),
                (no_game_prompt, no_game_flags),
                (unlock_support_reference, unlock_challenge_flags),
                (finish_message_values(text_code_units(title_mode_original)), title_mode_flags),
                (finish_message_values(text_code_units(DOWNLOAD_NO_GAME_MESSAGES[archive])), no_game_flags),
                (finish_message_values(text_code_units(DOWNLOAD_FAILED_MESSAGES[archive])), disconnect_flags),
                (finish_message_values(text_code_units(DOWNLOAD_START_MESSAGES[archive])), download_start_flags),
                (finish_message_values(text_code_units(UNLOCK_NO_RECOVERY_MESSAGES[archive])), game_selection_flags),
                (finish_message_values(text_code_units(UNLOCK_RECOVERED_MESSAGES[archive])), game_selection_flags),
                (finish_message_values(text_code_units(UNLOCK_FORCED_RECOVERED_MESSAGES[archive])), game_selection_flags),
                (finish_message_values(text_code_units(TICKET_INSUFFICIENT_FUNDS_MESSAGES[archive])), game_selection_flags),
                (finish_message_values(text_code_units(title_language)), title_mode_flags),
                (finish_message_values([]), title_mode_flags),  # Reserved former support-disabled line 123.
                *((finish_message_values(text_code_units(disabled_labels[target])),
                   message_codec.u16(original, section_offset + 4 + source * 8 + 6))
                  for source, target in DISABLED_MENU_LINES.items()),
                (finish_message_values(text_code_units(TITLE_SWITCH_MESSAGES[archive])), title_mode_flags),
                (finish_message_values(text_code_units(USER_LABEL_MESSAGES[archive])), title_mode_flags),
                (finish_message_values(text_code_units(USER_SWITCH_HINTS[archive])), title_mode_flags),
                (finish_message_values(text_code_units(build_invalid_support_label(archive, original_lines))),
                 message_codec.u16(original, section_offset + 4 + 43 * 8 + 6)),
                (offline_mismatch, mismatch_flags),
            ),
        )
        greeting_entry = entries[MENU_MESSAGE_FILE_INDEX]
        greeting_original = greeting_entry.files[0]
        greeting_section_offset = message_codec.u32(greeting_original, 12)
        greeting_flags = message_codec.u16(
            greeting_original, greeting_section_offset + 4 + MENU_GREETING_LINE * 8 + 6
        )
        # Keep the stock line untouched and append three complete mode-specific
        # alternatives. Each alternative starts with the stock greeting (or
        # its one-line equivalent) and then adds the selected-mode explanation.
        # 原版行保持不变，另行追加三条完整的模式文本。每条先放原版欢迎语（或其
        # 一行等义版本），再追加所选模式说明。
        greeting_original_lines = message_codec.read_message_lines(greeting_original)
        menu_greeting = SHORT_MENU_GREETINGS.get(
            archive, greeting_original_lines[MENU_GREETING_LINE]
        )
        offline_menu_greeting = (
            f"{menu_greeting}\n"
            f"{OFFLINE_MENU_GREETINGS[archive]}"
        )
        download_menu_greeting = (
            f"{menu_greeting}\n"
            f"{DOWNLOAD_MENU_GREETINGS[archive]}"
        )
        unlock_menu_greeting = (
            f"{menu_greeting}\n"
            f"{UNLOCK_MENU_GREETINGS[archive]}"
        )
        greeting_entry.files[0] = message_codec.patch_message_file(
            greeting_original,
            {},
            (
                (offline_menu_greeting, greeting_flags),
                (download_menu_greeting, greeting_flags),
                (unlock_menu_greeting, greeting_flags),
            ),
        )
        rebuilt = message_codec.write_garc(version, alignment, entries)
        rebuilt_entries = message_codec.read_garc(rebuilt)[2]
        rebuilt_lines = message_codec.read_message_lines(
            rebuilt_entries[MESSAGE_FILE_INDEX].files[0]
        )
        rebuilt_greetings = message_codec.read_message_lines(
            rebuilt_entries[MENU_MESSAGE_FILE_INDEX].files[0]
        )
        expected = {
            SUPPORT_LINE: original_lines[SUPPORT_LINE],
            MOVER_DOWNLOAD_LINE: original_lines[MOVER_DOWNLOAD_LINE],
            MOVER_INSTALLED_LINE: original_lines[MOVER_INSTALLED_LINE],
            HOME_LINE: original_lines[HOME_LINE],
            BLANK_LINE: "",
            DOWNLOAD_PROGRESS_LINE: DOWNLOAD_PROGRESS_MESSAGES[archive],
            DOWNLOAD_SUCCESS_LINE: DOWNLOAD_SUCCESS_MESSAGES[archive],
            DOWNLOAD_USE_BANK_LINE: DOWNLOAD_MENU_MESSAGES[archive],
            DOWNLOAD_NO_GAME_LINE: DOWNLOAD_NO_GAME_MESSAGES[archive],
            DOWNLOAD_FAILED_LINE: DOWNLOAD_FAILED_MESSAGES[archive],
            DOWNLOAD_START_LINE: DOWNLOAD_START_MESSAGES[archive],
            OFFLINE_INITIAL_CONNECT_LINE: OFFLINE_INITIAL_CONNECT_MESSAGES[archive],
            OFFLINE_BANK_CONNECTION_LINE: OFFLINE_BANK_CONNECTION_MESSAGES[archive],
            OFFLINE_SAVE_LINE: OFFLINE_SAVE_MESSAGES[archive],
            OFFLINE_DISCONNECT_LINE: OFFLINE_DISCONNECT_MESSAGES[archive],
            TITLE_MODE_OFFLINE_LINE: title_mode_offline,
            TITLE_MODE_DOWNLOAD_LINE: title_mode_download,
            RESERVED_DISABLED_LINE: "",
            RESERVED_SUPPORT_DISABLED_LINE: "",
            RESERVED_LANGUAGE_MENU_LINE: "",
            TITLE_MODE_UNLOCK_LINE: title_mode_unlock,
            TITLE_MODE_ORIGINAL_LINE: title_mode_original,
            UNLOCK_USE_BANK_LINE: UNLOCK_MENU_MESSAGES[archive],
            UNLOCK_GAME_SELECTION_LINE: UNLOCK_GAME_SELECTION_MESSAGES[archive],
            UNLOCK_NO_RECOVERY_LINE: UNLOCK_NO_RECOVERY_MESSAGES[archive],
            UNLOCK_RECOVERED_LINE: UNLOCK_RECOVERED_MESSAGES[archive],
            UNLOCK_FORCED_RECOVERED_LINE: UNLOCK_FORCED_RECOVERED_MESSAGES[archive],
            TICKET_INSUFFICIENT_FUNDS_LINE: TICKET_INSUFFICIENT_FUNDS_MESSAGES[archive],
            TITLE_LANGUAGE_LINE: title_language,
            TITLE_SWITCH_LINE: TITLE_SWITCH_MESSAGES[archive],
            **disabled_labels,
        }
        for index, value in expected.items():
            if rebuilt_lines[index] != value:
                raise ValueError(f"message verification failed for {archive}, line {index}")
        progress=DOWNLOAD_PROGRESS_MESSAGES[archive]
        if not 1<=len(progress.splitlines())<=3 or \
                max(len(line.encode("utf-16le"))//2 for line in progress.splitlines())>48 or \
                any(part in progress.lower() for part in ("bankdata.",":/","\\","/")):
            raise ValueError(f"download progress exceeds three lines or exposes a path in {archive}")
        if rebuilt_greetings[OFFLINE_MENU_GREETING_LINE] != offline_menu_greeting:
            raise ValueError(f"offline-menu greeting verification failed for {archive}")
        if rebuilt_greetings[DOWNLOAD_MENU_GREETING_LINE] != download_menu_greeting:
            raise ValueError(f"download-menu greeting verification failed for {archive}")
        if rebuilt_greetings[UNLOCK_MENU_GREETING_LINE] != unlock_menu_greeting:
            raise ValueError(f"unlock-menu greeting verification failed for {archive}")
        if message_codec.read_message_line_values(
            rebuilt_entries[MESSAGE_FILE_INDEX].files[0], NO_GAME_BLOCKED_LINE
        ) != no_game_prompt:
            raise ValueError(f"no-game prompt verification failed for {archive}")
        if message_codec.read_message_line_values(
            rebuilt_entries[MESSAGE_FILE_INDEX].files[0], UNLOCK_CHALLENGE_LINE
        ) != unlock_challenge:
            raise ValueError(f"unlock-challenge verification failed for {archive}")
        if message_codec.read_message_line_values(
            rebuilt_entries[MESSAGE_FILE_INDEX].files[0], OFFLINE_MISMATCH_LINE
        ) != offline_mismatch:
            raise ValueError(f"offline mismatch pagination verification failed for {archive}")
        for index in range(len(original_lines)):
            if message_codec.read_message_line_values(
                rebuilt_entries[MESSAGE_FILE_INDEX].files[0], index
            ) != message_codec.read_message_line_values(original, index):
                raise ValueError(f"stock message changed for {archive}, line {index}")
        if rebuilt_greetings[MENU_GREETING_LINE] != greeting_original_lines[MENU_GREETING_LINE]:
            raise ValueError(f"stock-menu greeting verification failed for {archive}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(rebuilt)
        print(f"patched {archive} -> {destination}")


if __name__ == "__main__":
    main()
