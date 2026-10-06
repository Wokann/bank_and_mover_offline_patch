#!/usr/bin/env python3
"""Append mode-specific messages for the combined Poke Mover patch.

为 Poke Mover 合并补丁追加按模式选择的文本。
"""

from __future__ import annotations

import argparse
from pathlib import Path

from tools import message_archive as message_codec


ARCHIVES = ("0/0/4", "0/0/5", "0/0/6", "0/0/7", "0/0/8", "0/0/9",
            "0/1/0", "0/1/1", "0/1/2", "0/1/3")
MESSAGE_FILE_INDEX = 23
TITLE_HOME_LINE = 39
TITLE_OFFLINE_LINE = 67
TITLE_ONLINE_LINE = 68
STOCK_INITIAL_CONNECT_LINE = 13
STOCK_BANK_CONNECT_LINE = 15
STOCK_SAVE_LINE = 9
STOCK_DISCONNECT_LINE = 14
INITIAL_CONNECT_LINE = 69
BANK_CONNECT_LINE = 70
SAVE_LINE = 71
DISCONNECT_LINE = 72
TITLE_TEXT_BUFFER_LENGTH = 54
R_BUTTON = "\ue005"

INTERNET_MESSAGES = {
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

BANK_CONNECTION_MESSAGES = {
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

SAVE_MESSAGES = {
    "0/0/4": "レポートを　かいて\nポケモンを　ローカル オフラインデータに\nほぞん　しています\nでんげんを　きらないで　ください",
    "0/0/5": "レポートを　書いて\nポケモンをローカルオフラインデータに\n保存しています\n電源を　切らないで　ください",
    "0/0/6": "Your game is being saved and your Pokémon moved\nto the local offline Bank data.\nDon’t turn off the power.",
    "0/0/7": "Sauvegarde du jeu et transfert des Pokémon\nvers les données locales hors ligne…\nNe pas éteindre la console.",
    "0/0/8": "Salvataggio del gioco e dei Pokémon\nnei dati offline locali in corso.\nNon spegnere la console.",
    "0/0/9": "Spielstand und Pokémon werden in den lokalen\nOffline-Bankdaten gespeichert...\nBitte das System nicht ausschalten.",
    "0/1/0": "Guardando la partida y los Pokémon en los datos\nlocales sin conexión del Banco...\nNo apagues la consola.",
    "0/1/1": "리포트를 기록하고 포켓몬을\n로컬 오프라인 데이터에 저장하고 있습니다\n전원을 끄지 않도록 해주십시오",
    "0/1/2": "正在写入记录，\n并将宝可梦写入本地离线数据。\n请勿切断电源。",
    "0/1/3": "正在寫入記錄，\n並將寶可夢寫入本機離線資料。\n請勿關閉電源。",
}

DISCONNECT_MESSAGES = {
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

TITLE_HOME_SHORT = {
    "0/0/4": "\ue073を押すとHOMEメニューに戻ります",
    "0/0/5": "\ue073を押すとHOMEメニューに戻ります",
    "0/0/6": "Press \ue073 to HOME Menu",
    "0/0/7": "\ue073 : menu HOME",
    "0/0/8": "\ue073: menu HOME",
    "0/0/9": "\ue073: HOME-Menü",
    "0/1/0": "\ue073: menú HOME",
    "0/1/1": "\ue073을 누르면 HOME 메뉴로 돌아갑니다",
    "0/1/2": "如果按\ue073，就会返回HOME菜单。",
    "0/1/3": "按\ue073可返回HOME選單",
}

TITLE_OFFLINE = {
    "0/0/4": f"現在のモード：オフライン（{R_BUTTON}で切替）",
    "0/0/5": f"現在のモード：オフライン（{R_BUTTON}で切替）",
    "0/0/6": f"Current: Offline ({R_BUTTON}:switch mode)",
    "0/0/7": f"Mode : hors ligne ({R_BUTTON} : changer)",
    "0/0/8": f"Modalità: offline ({R_BUTTON}: cambia modo)",
    "0/0/9": f"Modus: Offline ({R_BUTTON}: Modus wechseln)",
    "0/1/0": f"Modo: sin conexión ({R_BUTTON}: cambiar modo)",
    "0/1/1": f"현재 모드: 오프라인 ({R_BUTTON}로 전환)",
    "0/1/2": f"当前模式：离线模式（按{R_BUTTON}键切换模式）",
    "0/1/3": f"目前模式：離線模式（按{R_BUTTON}鍵切換模式）",
}

TITLE_ONLINE = {
    "0/0/4": f"現在のモード：オンライン（{R_BUTTON}で切替）",
    "0/0/5": f"現在のモード：オンライン（{R_BUTTON}で切替）",
    "0/0/6": f"Current: Online ({R_BUTTON}:switch mode)",
    "0/0/7": f"Mode : en ligne ({R_BUTTON} : changer)",
    "0/0/8": f"Modalità: online ({R_BUTTON}: cambia modo)",
    "0/0/9": f"Modus: Online ({R_BUTTON}: Modus wechseln)",
    "0/1/0": f"Modo: en línea ({R_BUTTON}: cambiar modo)",
    "0/1/1": f"현재 모드: 온라인 ({R_BUTTON}로 전환)",
    "0/1/2": f"当前模式：在线模式（按{R_BUTTON}键切换模式）",
    "0/1/3": f"目前模式：在線模式（按{R_BUTTON}鍵切換模式）",
}


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-romfs", required=True, type=Path)
    parser.add_argument("--output-romfs", required=True, type=Path)
    args = parser.parse_args()

    for archive in ARCHIVES:
        source = args.source_romfs / "a" / Path(archive)
        destination = args.output_romfs / "a" / Path(archive)
        version, alignment, entries = message_codec.read_garc(source.read_bytes())
        entry = entries[MESSAGE_FILE_INDEX]
        original = entry.files[0]
        original_lines = message_codec.read_message_lines(original)
        if len(original_lines) != TITLE_OFFLINE_LINE:
            raise ValueError(f"unexpected stock line count for {archive}: {len(original_lines)}")
        section_offset = message_codec.u32(original, 12)

        def flags(line: int) -> int:
            return message_codec.u16(original, section_offset + 4 + line * 8 + 6)

        offline_title = f"{TITLE_HOME_SHORT[archive]}\n{TITLE_OFFLINE[archive]}"
        online_title = f"{TITLE_HOME_SHORT[archive]}\n{TITLE_ONLINE[archive]}"
        for mode, text in (("offline", offline_title), ("online", online_title)):
            if len(text) > TITLE_TEXT_BUFFER_LENGTH:
                raise ValueError(
                    f"{archive} {mode} title text exceeds "
                    f"{TITLE_TEXT_BUFFER_LENGTH} characters: {len(text)}"
                )

        entry.files[0] = message_codec.patch_message_file(
            original,
            {},
            (
                (offline_title, flags(TITLE_HOME_LINE)),
                (online_title, flags(TITLE_HOME_LINE)),
                (INTERNET_MESSAGES[archive], flags(STOCK_INITIAL_CONNECT_LINE)),
                (BANK_CONNECTION_MESSAGES[archive], flags(STOCK_BANK_CONNECT_LINE)),
                (SAVE_MESSAGES[archive], flags(STOCK_SAVE_LINE)),
                (DISCONNECT_MESSAGES[archive], flags(STOCK_DISCONNECT_LINE)),
            ),
        )
        rebuilt = message_codec.write_garc(version, alignment, entries)
        rebuilt_lines = message_codec.read_message_lines(
            message_codec.read_garc(rebuilt)[2][MESSAGE_FILE_INDEX].files[0]
        )
        expected = {
            TITLE_HOME_LINE: original_lines[TITLE_HOME_LINE],
            TITLE_OFFLINE_LINE: offline_title,
            TITLE_ONLINE_LINE: online_title,
            INITIAL_CONNECT_LINE: INTERNET_MESSAGES[archive],
            BANK_CONNECT_LINE: BANK_CONNECTION_MESSAGES[archive],
            SAVE_LINE: SAVE_MESSAGES[archive],
            DISCONNECT_LINE: DISCONNECT_MESSAGES[archive],
        }
        for line, value in expected.items():
            if rebuilt_lines[line] != value:
                raise ValueError(f"message verification failed for {archive}, line {line}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(rebuilt)
        print(f"patched {archive} -> {destination}")


if __name__ == "__main__":
    main()
