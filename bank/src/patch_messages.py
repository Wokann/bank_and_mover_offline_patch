"""Build the ten localized resources used by the combined Bank patch.

构建合并 Bank 补丁使用的十套本地化资源。
"""

from __future__ import annotations

import argparse
from pathlib import Path

import message_archive as message_codec


ARCHIVES = (
    "0/0/4", "0/0/5", "0/0/6", "0/0/7", "0/0/8",
    "0/0/9", "0/1/0", "0/1/1", "0/1/2", "0/1/3",
)
MESSAGE_FILE_INDEX = 39
INTERNET_CONNECTION_LINE = 12
BANK_CONNECTION_LINE = 14
SAVE_LINE = 8
DISCONNECT_LINE = 13
MENU_LINE = 41
UNLOCK_CHALLENGE_SOURCE_LINE = 34

DOWNLOAD_PROGRESS_MESSAGES = {
    "0/0/4": "ポケモンバンクからSDへ\nダウンロードしています\nsd:/3ds/Bank/bankdata.bin",
    "0/0/5": "ポケモンバンクからSDへダウンロード中\nsd:/3ds/Bank/bankdata.bin",
    "0/0/6": "Downloading from Pokémon Bank to the local SD card...\nsd:/3ds/Bank/bankdata.bin",
    "0/0/7": "Téléchargement de Banque Pokémon vers la carte SD…\nsd:/3ds/Bank/bankdata.bin",
    "0/0/8": "Download dalla Banca Pokémon alla scheda SD…\nsd:/3ds/Bank/bankdata.bin",
    "0/0/9": "Bankdaten werden auf die SD-Karte heruntergeladen…\nsd:/3ds/Bank/bankdata.bin",
    "0/1/0": "Descargando del Banco de Pokémon a la tarjeta SD…\nsd:/3ds/Bank/bankdata.bin",
    "0/1/1": "포켓몬 뱅크에서 SD 카드로\n다운로드 중입니다.\nsd:/3ds/Bank/bankdata.bin",
    "0/1/2": "正在从宝可梦虚拟银行下载数据到本地 SD 卡：\nsd:/3ds/Bank/bankdata.bin",
    "0/1/3": "正在從寶可夢虛擬銀行下載資料到本機 SD 卡：\nsd:/3ds/Bank/bankdata.bin",
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

LANGUAGE_MENU_MESSAGES = {
    "0/0/4": "げんごを　えらぶ",
    "0/0/5": "言語を選ぶ",
    "0/0/6": "Choose Language",
    "0/0/7": "Choisir la langue",
    "0/0/8": "Scegli la lingua",
    "0/0/9": "Sprache wählen",
    "0/1/0": "Elegir idioma",
    "0/1/1": "언어 선택",
    "0/1/2": "选择语言",
    "0/1/3": "選擇語言",
}

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
    "0/0/4": "ローカル オフラインデータに\nせつぞくしています……",
    "0/0/5": "ローカルオフラインデータに\n接続しています……",
    "0/0/6": "Communicating with the local offline\nPokemon Bank data...",
    "0/0/7": "Connexion aux données locales hors ligne\nde Banque Pokémon…",
    "0/0/8": "Connessione ai dati locali offline\ndella Banca Pokémon...",
    "0/0/9": "Verbindung mit den lokalen Offline-Daten\nder Pokémon Bank...",
    "0/1/0": "Conectando con los datos locales sin conexión\ndel Banco de Pokémon...",
    "0/1/1": "로컬 오프라인 포켓몬 뱅크 데이터에\n연결 중입니다…",
    "0/1/2": "正在和宝可梦虚拟银行的\n本地离线数据进行连接……",
    "0/1/3": "正在和寶可夢虛擬銀行的\n本機離線資料進行連線……",
}

OFFLINE_SAVE_MESSAGES = {
    "0/0/4": "レポートを　かいて\nローカル オフラインデータに　ほぞん　しています\nでんげんを　きらないで　ください",
    "0/0/5": "レポートを　書いて\nローカルオフラインデータに保存しています\n電源を　切らないで　ください",
    "0/0/6": "Saving the data to the local offline file...\nDon’t turn off the power.",
    "0/0/7": "Sauvegarde des données dans le fichier hors ligne local…\nNe pas éteindre la console.",
    "0/0/8": "Salvataggio dei dati nel file offline locale in corso.\nNon spegnere la console.",
    "0/0/9": "Daten werden in der lokalen Offline-Datei gespeichert...\nBitte das System nicht ausschalten.",
    "0/1/0": "Guardando los datos en el archivo local sin conexión...\nNo apagues la consola.",
    "0/1/1": "리포트를 기록하고\n로컬 오프라인 데이터에 저장하고 있습니다\n전원을 끄지 않도록 해주십시오",
    "0/1/2": "正在写入记录，\n并将数据写入本地离线文件。\n请勿切断电源。",
    "0/1/3": "正在寫入記錄，\n並將資料寫入本機離線檔案。\n請勿關閉電源。",
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
DISABLED_LINE = 106
LANGUAGE_MENU_LINE = 107
DOWNLOAD_GAME_SELECTION_LINE = 108
TITLE_MODE_UNLOCK_LINE = 109
UNLOCK_USE_BANK_LINE = 110
UNLOCK_GAME_SELECTION_LINE = 111
UNLOCK_CHALLENGE_LINE = 112
TITLE_TEXT_BUFFER_LENGTH = 54

DISABLED_MESSAGES = {
    "0/0/4": "つかえません",
    "0/0/5": "使用できません",
    "0/0/6": "Disabled",
    "0/0/7": "Indisponible",
    "0/0/8": "Non disponibile",
    "0/0/9": "Deaktiviert",
    "0/1/0": "Desactivado",
    "0/1/1": "사용할 수 없습니다",
    "0/1/2": "已禁用",
    "0/1/3": "已停用",
}

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
L_BUTTON = "\ue004"
A_BUTTON = "\ue000"

# The Western stock HOME sentences leave too little room for a localized
# mode-switch instruction inside the title pane's 54-character buffer.
# 西欧语言的原版 HOME 句子会挤占标题窗格的 54 字符缓冲区，因此使用本地化短句为
# 完整的模式切换说明留出空间。
SHORT_TITLE_HOME_MESSAGES = {
    "0/0/6": "Press\ue073: HOME Menu",
    "0/0/7": "Appuyez\ue073: menu HOME",
    "0/0/8": "Premi\ue073: menu HOME",
    "0/0/9": "\ue073drücken: HOME-Menü",
    "0/1/0": "Pulsa\ue073: menú HOME",
}

# Three appended BMG entries replace the wide bottom HOME-help line while the
# stock version pane remains unchanged. The title hook rebinds this line after
# each R press.
# 三条追加 BMG 条目替换宽大的底部 HOME 帮助行，原版版本号窗格保持不变。标题钩子会在
# 每次按下 R 后重新绑定该行。
TITLE_MODE_OFFLINE_MESSAGES = {
    "0/0/4": f"現在のモード：オフライン（{R_BUTTONS['0/0/4']}で切替）",
    "0/0/5": f"現在のモード：オフライン（{R_BUTTONS['0/0/5']}で切替）",
    "0/0/6": f"Mode: Offline ({R_BUTTONS['0/0/6']}:switch mode)",
    "0/0/7": f"Mode: Hors ligne ({R_BUTTONS['0/0/7']}:changer)",
    "0/0/8": f"Modalità: Offline ({R_BUTTONS['0/0/8']}:cambia modo)",
    "0/0/9": f"Modus: Offline({R_BUTTONS['0/0/9']}:Modus wechseln)",
    "0/1/0": f"Modo: Sin conexión ({R_BUTTONS['0/1/0']}:cambiar modo)",
    "0/1/1": f"현재 모드: 오프라인 ({R_BUTTONS['0/1/1']}으로 전환)",
    "0/1/2": f"当前模式：离线模式（按{R_BUTTONS['0/1/2']}键切换模式）",
    "0/1/3": f"目前模式：離線模式（按{R_BUTTONS['0/1/3']}鍵切換模式）",
}

TITLE_MODE_DOWNLOAD_MESSAGES = {
    "0/0/4": f"現在のモード：ダウンロード（{R_BUTTONS['0/0/4']}で切替）",
    "0/0/5": f"現在のモード：ダウンロード（{R_BUTTONS['0/0/5']}で切替）",
    "0/0/6": f"Mode: Download ({R_BUTTONS['0/0/6']}:switch mode)",
    "0/0/7": f"Mode: Téléchargement ({R_BUTTONS['0/0/7']}:changer)",
    "0/0/8": f"Modalità: Download ({R_BUTTONS['0/0/8']}:cambia modo)",
    "0/0/9": f"Modus: Download({R_BUTTONS['0/0/9']}:Modus wechseln)",
    "0/1/0": f"Modo: Descarga ({R_BUTTONS['0/1/0']}:cambiar modo)",
    "0/1/1": f"현재 모드: 다운로드 ({R_BUTTONS['0/1/1']}으로 전환)",
    "0/1/2": f"当前模式：下载模式（按{R_BUTTONS['0/1/2']}键切换模式）",
    "0/1/3": f"目前模式：下載模式（按{R_BUTTONS['0/1/3']}鍵切換模式）",
}

TITLE_MODE_UNLOCK_MESSAGES = {
    "0/0/4": f"現在のモード：アンロック（{R_BUTTONS['0/0/4']}で切替）",
    "0/0/5": f"現在のモード：アンロック（{R_BUTTONS['0/0/5']}で切替）",
    "0/0/6": f"Mode: Unlock ({R_BUTTONS['0/0/6']}:switch mode)",
    "0/0/7": f"Mode: Déverrouillage ({R_BUTTONS['0/0/7']}:changer)",
    "0/0/8": f"Modalità: Sblocco ({R_BUTTONS['0/0/8']}:cambia modo)",
    "0/0/9": f"Modus: Entsperren({R_BUTTONS['0/0/9']}:wechseln)",
    "0/1/0": f"Modo: Desbloqueo ({R_BUTTONS['0/1/0']}:cambiar modo)",
    "0/1/1": f"현재 모드: 잠금 해제 ({R_BUTTONS['0/1/1']}으로 전환)",
    "0/1/2": f"当前模式：解锁模式（按{R_BUTTONS['0/1/2']}键切换模式）",
    "0/1/3": f"目前模式：解鎖模式（按{R_BUTTONS['0/1/3']}鍵切換模式）",
}

OFFLINE_MENU_GREETINGS = {
    "0/0/4": "げんざいのモード：オフラインモード\nローカルデータは サーバーデータと べつです",
    "0/0/5": "現在のモード：オフラインモード\nローカルデータはサーバーデータと別です",
    "0/0/6": "Current Mode: Offline Mode\nLocal Bank data is separate from server data.",
    "0/0/7": "Mode actuel : Mode hors ligne\nLes données locales sont séparées du serveur.",
    "0/0/8": "Modalità attuale: Offline\nI dati locali sono separati dal server.",
    "0/0/9": "Aktueller Modus: Offline\nLokale Daten sind von Serverdaten getrennt.",
    "0/1/0": "Modo actual: Sin conexión\nLos datos locales son independientes del servidor.",
    "0/1/1": "현재 모드: 오프라인 모드\n로컬 뱅크 데이터는 서버 데이터와 분리됩니다.",
    "0/1/2": "当前模式：离线模式\n本地银行数据与服务器数据彼此独立。",
    "0/1/3": "目前模式：離線模式\n本機銀行資料與伺服器資料彼此獨立。",
}

DOWNLOAD_MENU_GREETINGS = {
    "0/0/4": "げんざいのモード：ダウンロードモード\nサーバーのデータを ほんたいに ほぞんして うわがきします",
    "0/0/5": "現在のモード：ダウンロードモード\nサーバーのデータを本体に保存して上書きします",
    "0/0/6": "Current Mode: Download Mode\nServer data is downloaded to SD and overwrites local data.",
    "0/0/7": "Mode actuel : Téléchargement\nLes données du serveur remplaceront les données locales.",
    "0/0/8": "Modalità attuale: Download\nI dati del server sovrascriveranno quelli locali.",
    "0/0/9": "Aktueller Modus: Download\nServerdaten überschreiben lokale Bankdaten.",
    "0/1/0": "Modo actual: Descarga\nLos datos del servidor reemplazarán los datos locales.",
    "0/1/1": "현재 모드: 다운로드 모드\n서버 데이터를 로컬 뱅크 데이터에 덮어씁니다.",
    "0/1/2": "当前模式：下载模式\n将服务器银行数据下载到本地并覆盖现有数据。",
    "0/1/3": "目前模式：下載模式\n將伺服器銀行資料下載到本機並覆蓋現有資料。",
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
    "0/0/4": "ぎんこうデータを SDカードに\nダウンロードしました\nタイトルにもどります……",
    "0/0/5": "銀行データをSDカードに\nダウンロードしました\nタイトルに戻ります……",
    "0/0/6": "Bank data was downloaded to the local SD card.\nReturning to the title screen...",
    "0/0/7": "Les données de Banque ont été téléchargées\nsur la carte SD locale. Retour à l’écran-titre…",
    "0/0/8": "I dati della Banca sono stati scaricati\nsulla scheda SD locale. Ritorno al titolo...",
    "0/0/9": "Bankdaten wurden auf die lokale SD-Karte\ngeladen. Rückkehr zum Titelbildschirm...",
    "0/1/0": "Los datos del Banco se descargaron\nen la tarjeta SD local. Volviendo al título...",
    "0/1/1": "뱅크 데이터를 로컬 SD 카드에\n다운로드했습니다. 타이틀 화면으로 돌아갑니다…",
    "0/1/2": "已将银行数据下载到本地 SD 卡。\n正在返回标题界面……",
    "0/1/3": "已將銀行資料下載到本機 SD 卡。\n正在返回標題畫面……",
}

DOWNLOAD_GAME_SELECTION_MESSAGES = {
    "0/0/4": "どのゲームソフトでも\nかんぜんな ぎんこうデータを\nSDカードに ダウンロードできます。",
    "0/0/5": "どのゲームソフトを選んでも\n完全な銀行データを本体に\nダウンロードできます。",
    "0/0/6": "Select any game software to download\nthe complete Bank data to local storage.",
    "0/0/7": "Sélectionnez n’importe quel jeu pour\ntélécharger toutes les données de Banque\nen local.",
    "0/0/8": "Seleziona un gioco qualsiasi per\nscaricare localmente tutti i dati\ndella Banca.",
    "0/0/9": "Wähle ein beliebiges Spiel, um\nalle Bankdaten lokal herunterzuladen.",
    "0/1/0": "Elige cualquier juego para descargar\nlocalmente todos los datos del Banco.",
    "0/1/1": "어떤 게임 소프트웨어를 선택해도\n전체 뱅크 데이터를 로컬에\n다운로드할 수 있습니다.",
    "0/1/2": "选择任意游戏软件，\n均可将完整银行数据下载到本地。",
    "0/1/3": "選擇任意遊戲軟體，\n均可將完整銀行資料下載到本機。",
}

UNLOCK_GAME_SELECTION_MESSAGES = {
    "0/0/4": f"ゲームをえらんで ボタンをおすとき、\n{L_BUTTON} + {A_BUTTON} + STARTを おしつづけて\nアンロックモードに はいります。",
    "0/0/5": f"ゲームを選んでボタンを押すとき、\n{L_BUTTON} + {A_BUTTON} + STARTを押し続けて\nアンロックモードに入ります。",
    "0/0/6": f"When confirming a game,\nhold {L_BUTTON} + {A_BUTTON} + START to enter Unlock Mode.",
    "0/0/7": f"En confirmant un jeu,\nmaintenez {L_BUTTON} + {A_BUTTON} + START pour ouvrir\nle mode Déverrouillage.",
    "0/0/8": f"Quando confermi un gioco,\ntieni {L_BUTTON} + {A_BUTTON} + START per aprire\nla modalità Sblocco.",
    "0/0/9": f"Beim Bestätigen eines Spiels\n{L_BUTTON} + {A_BUTTON} + START gedrückt halten,\num den Entsperrmodus zu öffnen.",
    "0/1/0": f"Al confirmar un juego,\nmantén {L_BUTTON} + {A_BUTTON} + START para abrir\nel modo Desbloqueo.",
    "0/1/1": f"게임을 선택해 버튼을 누를 때\n{L_BUTTON} + {A_BUTTON} + START를 계속 눌러\n잠금 해제 모드로 들어갑니다.",
    "0/1/2": f"选择游戏按下按钮的同时，\n按住{L_BUTTON} + {A_BUTTON} + START 以进入解锁模式。",
    "0/1/3": f"選擇遊戲按下按鈕的同時，\n按住{L_BUTTON} + {A_BUTTON} + START 以進入解鎖模式。",
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


def verify_kana_archive_messages() -> None:
    """Reject kanji in every custom string emitted for the kana-only archive.

    拒绝写入假名专用文本库的任何自定义汉字。
    """
    archive = "0/0/4"
    # The stock HOME hint in this archive already contains kanji. Its two
    # appended title-mode variants intentionally follow the same convention;
    # the remaining custom messages stay kana-only.
    # 此档案的原版 HOME 提示本身已包含汉字，因此追加的三条标题模式文本也有意
    # 使用相同写法；其余自定义文本仍保持纯假名。
    texts = (
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
        UNLOCK_MENU_MESSAGES[archive],
        LANGUAGE_MENU_MESSAGES[archive],
        DOWNLOAD_GAME_SELECTION_MESSAGES[archive],
        UNLOCK_MENU_GREETINGS[archive],
        UNLOCK_GAME_SELECTION_MESSAGES[archive],
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
    verify_kana_archive_messages()

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
        unlock_support_reference = build_support_reference_values(
            unlock_challenge_source, UNLOCK_SUPPORT_REFERENCE_LABELS[archive]
        )
        unlock_challenge = build_unlock_prompt_values(
            unlock_challenge_source,
            UNLOCK_SUPPORT_REFERENCE_LABELS[archive],
            UNLOCK_CODE_LABELS[archive],
        )
        if args.title_r_glyph_test:
            # Diagnostic archive: isolate private-use glyph rendering from
            # multiline layout and localized text length.
            # 诊断档案：将专用区字形渲染与多行布局、本地化文本长度完全分离。
            title_mode_offline = original_lines[TITLE_HOME_LINE] + R_BUTTONS[archive]
            title_mode_download = title_mode_offline
            title_mode_unlock = title_mode_offline
        else:
            title_home = SHORT_TITLE_HOME_MESSAGES.get(
                archive, original_lines[TITLE_HOME_LINE]
            )
            title_mode_offline = (
                f"{title_home}\n"
                f"{TITLE_MODE_OFFLINE_MESSAGES[archive]}"
            )
            title_mode_download = (
                f"{title_home}\n"
                f"{TITLE_MODE_DOWNLOAD_MESSAGES[archive]}"
            )
            title_mode_unlock = (
                f"{title_home}\n"
                f"{TITLE_MODE_UNLOCK_MESSAGES[archive]}"
            )
        # The stock title TextBox reserves 54 UTF-16 characters. The renderer
        # clears the whole pane instead of truncating an oversized string.
        # 原版标题 TextBox 仅预留 54 个 UTF-16 字符；越界时渲染器会清空整个
        # 文本窗格，而不是截断字符串。
        for mode_name, title_text in (
            ("offline", title_mode_offline),
            ("download", title_mode_download),
            ("unlock", title_mode_unlock),
        ):
            if len(title_text) > TITLE_TEXT_BUFFER_LENGTH:
                raise ValueError(
                    f"{archive} {mode_name} title text exceeds "
                    f"{TITLE_TEXT_BUFFER_LENGTH} characters: {len(title_text)}"
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
                (DISABLED_MESSAGES[archive], use_bank_flags),
                (LANGUAGE_MENU_MESSAGES[archive], use_bank_flags),
                (DOWNLOAD_GAME_SELECTION_MESSAGES[archive], game_selection_flags),
                (title_mode_unlock, title_mode_flags),
                (UNLOCK_MENU_MESSAGES[archive], use_bank_flags),
                (UNLOCK_GAME_SELECTION_MESSAGES[archive], game_selection_flags),
            ),
            ((unlock_challenge, unlock_challenge_flags),),
            value_replacements={
                UNLOCK_CHALLENGE_SOURCE_LINE: unlock_support_reference,
            },
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
            OFFLINE_INITIAL_CONNECT_LINE: OFFLINE_INITIAL_CONNECT_MESSAGES[archive],
            OFFLINE_BANK_CONNECTION_LINE: OFFLINE_BANK_CONNECTION_MESSAGES[archive],
            OFFLINE_SAVE_LINE: OFFLINE_SAVE_MESSAGES[archive],
            OFFLINE_DISCONNECT_LINE: OFFLINE_DISCONNECT_MESSAGES[archive],
            TITLE_MODE_OFFLINE_LINE: title_mode_offline,
            TITLE_MODE_DOWNLOAD_LINE: title_mode_download,
            DISABLED_LINE: DISABLED_MESSAGES[archive],
            LANGUAGE_MENU_LINE: LANGUAGE_MENU_MESSAGES[archive],
            DOWNLOAD_GAME_SELECTION_LINE: DOWNLOAD_GAME_SELECTION_MESSAGES[archive],
            TITLE_MODE_UNLOCK_LINE: title_mode_unlock,
            UNLOCK_USE_BANK_LINE: UNLOCK_MENU_MESSAGES[archive],
            UNLOCK_GAME_SELECTION_LINE: UNLOCK_GAME_SELECTION_MESSAGES[archive],
        }
        for index, value in expected.items():
            if rebuilt_lines[index] != value:
                raise ValueError(f"message verification failed for {archive}, line {index}")
        if rebuilt_greetings[OFFLINE_MENU_GREETING_LINE] != offline_menu_greeting:
            raise ValueError(f"offline-menu greeting verification failed for {archive}")
        if rebuilt_greetings[DOWNLOAD_MENU_GREETING_LINE] != download_menu_greeting:
            raise ValueError(f"download-menu greeting verification failed for {archive}")
        if rebuilt_greetings[UNLOCK_MENU_GREETING_LINE] != unlock_menu_greeting:
            raise ValueError(f"unlock-menu greeting verification failed for {archive}")
        if message_codec.read_message_line_values(
            rebuilt_entries[MESSAGE_FILE_INDEX].files[0], UNLOCK_CHALLENGE_LINE
        ) != unlock_challenge:
            raise ValueError(f"unlock-challenge verification failed for {archive}")
        if rebuilt_greetings[MENU_GREETING_LINE] != greeting_original_lines[MENU_GREETING_LINE]:
            raise ValueError(f"stock-menu greeting verification failed for {archive}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(rebuilt)
        print(f"patched {archive} -> {destination}")


if __name__ == "__main__":
    main()
