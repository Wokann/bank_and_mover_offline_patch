#!/usr/bin/env python3
"""Append mode-specific messages and ROM-language Gen 5 titles.

为 Poke Mover 合并补丁追加按模式选择的文本及按 ROM 语言显示的第五世代游戏名。
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from tools import message_archive as message_codec
from tools.title_layout import build_title_archive
from patch_version import add_version_arguments, label_from_arguments
from title_font import build_title_fonts
from title_messages import L_BUTTON, language_hint, mode_messages, mode_switch_hint, USER_LABEL_MESSAGES, USER_SWITCH_HINTS
from storage_messages import storage_notice_lines, verify_storage_notice_fonts


ARCHIVES = ("0/0/4", "0/0/5", "0/0/6", "0/0/7", "0/0/8", "0/0/9",
            "0/1/0", "0/1/1", "0/1/2", "0/1/3")
MESSAGE_FILE_INDEX = 23

GAME_TITLE_FILE_INDEX = 24
GAME_TITLE_MESSAGE_BASE = 54
# Same order as languageRank() in nds_sources.c: J / O / F / I / D / S / K.
# 与 nds_sources.c 中 languageRank() 的 J / O / F / I / D / S / K 顺序一致。
GAME_TITLE_ARCHIVES = ("0/0/5", "0/0/6", "0/0/7", "0/0/8", "0/0/9",
                       "0/1/0", "0/1/1")
TITLE_HOME_LINE = 39
TITLE_LANGUAGE_LINE = 40
TITLE_SWITCH_LINE = 73
TITLE_USER_LABEL_LINE, TITLE_USER_HINT_LINE = 75, 76
TITLE_SWITCH_MESSAGES = {archive: mode_switch_hint(archive) for archive in ARCHIVES}
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
MISSING_BANK_DISCONNECT_LINE = 74

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

SAVE_MESSAGES = {
    "0/0/4": "レポートを　かいて　ポケモンを\nローカルサーバーに　てんそうしています\nでんげんを　きらないで　ください",
    "0/0/5": "レポートを　書いて　ポケモンを\nローカルサーバーに　転送しています\n電源を　切らないで　ください",
    "0/0/6": "Your game is being saved and your Pokémon moved\nto the local server.\nDon’t turn off the power.",
    "0/0/7": "Sauvegarde du jeu et transfert des Pokémon\nvers le serveur local…\nNe pas éteindre la console.",
    "0/0/8": "Salvataggio del gioco e dei Pokémon\nsul server locale in corso.\nNon spegnere la console.",
    "0/0/9": "Spielstand und Pokémon werden auf dem\nlokalen Server gespeichert...\nBitte das System nicht ausschalten.",
    "0/1/0": "Guardando la partida y los Pokémon\nen el servidor local...\nNo apagues la consola.",
    "0/1/1": "리포트를 기록하고 포켓몬을\n로컬 서버에 전송하고 있습니다\n전원을 끄지 않도록 해주십시오",
    "0/1/2": "正在写入记录，\n并将宝可梦传送到本地服务器。\n请勿切断电源。",
    "0/1/3": "正在寫入記錄，\n並將寶可夢傳送到本機伺服器。\n請勿關閉電源。",
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

# Only FS_MISSING from the offline Bank download selects this extra line.
# 仅离线银行下载明确返回 FS_MISSING 时选择此提示，其他断开及在线原文不变。
MISSING_BANK_DISCONNECT_MESSAGES = {
    "0/0/4": "さきに　ポケモンバンクで　しょきかしてから\nポケムーバーを　ごりようください。\nせつだん　しています……",
    "0/0/5": "先にポケモンバンクでデータを初期化してから\nポケムーバーをご利用ください。\n接続を切っています……",
    "0/0/6": "Please initialize your data in Pokémon Bank\nbefore using Poké Transporter.\nDisconnecting...",
    "0/0/7": "Initialisez vos données dans Banque Pokémon\navant d’utiliser Poké Transfert.\nDéconnexion…",
    "0/0/8": "Inizializza i dati nella Banca Pokémon\nprima di usare il Pokétrasferitore.\nDisconnessione…",
    "0/0/9": "Initialisiere deine Daten in Pokémon Bank,\nbevor du den PokéMover verwendest.\nVerbindung wird getrennt...",
    "0/1/0": "Inicializa tus datos en el Banco de Pokémon\nantes de usar el Poké Trasladador.\nDesconectando...",
    "0/1/1": "먼저 포켓몬 뱅크에서 데이터를 초기화한 후\n포켓무버를 이용해 주십시오.\n연결을 종료하는 중입니다…",
    "0/1/2": "请先登录宝可梦虚拟银行完成数据初始化，\n再使用宝可梦虚拟传送。\n正在断开连接……",
    "0/1/3": "請先登入寶可夢虛擬銀行完成資料初始化，\n再使用寶可夢虛擬傳送。\n正在中斷連線……",
}

TITLE_OFFLINE = mode_messages("offline")
TITLE_ONLINE = mode_messages("online")


def read_game_titles(source_romfs: Path) -> tuple[tuple[list[int], int], ...]:
    titles: list[tuple[list[int], int]] = []
    for archive in GAME_TITLE_ARCHIVES:
        entries = message_codec.read_garc((source_romfs / "a" / Path(archive)).read_bytes())[2]
        lines = message_codec.read_message_values(entries[GAME_TITLE_FILE_INDEX].files[0])
        if len(lines) != GAME_TITLE_MESSAGE_BASE:
            raise ValueError(f"unexpected stock game-title count for {archive}: {len(lines)}")
        titles.extend(lines[4:8])
    return tuple(titles)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-romfs", required=True, type=Path)
    parser.add_argument("--output-romfs", required=True, type=Path)
    add_version_arguments(parser)
    args = parser.parse_args()
    subprocess.run([sys.executable, "-B", str(Path(__file__).absolute().parents[2] /
                    "common/tools/check_file_links.py"), "--recursive", "--allow-missing",
                    *(str(path) for path in vars(args).values() if isinstance(path, Path))], check=True)

    game_titles = read_game_titles(args.source_romfs)
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

        offline_title = TITLE_OFFLINE[archive]
        online_title = TITLE_ONLINE[archive]
        for mode, text in (("offline", offline_title), ("online", online_title)):
            if len(text.encode("utf-16le")) > 254 or len(text.splitlines()) != 1:
                raise ValueError(
                    f"{archive} {mode} title text exceeds "
                    f"127 UTF-16 units or is not one line: {len(text)}"
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
                (TITLE_SWITCH_MESSAGES[archive], flags(TITLE_HOME_LINE)),
                (MISSING_BANK_DISCONNECT_MESSAGES[archive], flags(STOCK_DISCONNECT_LINE)),
                (USER_LABEL_MESSAGES[archive], flags(TITLE_HOME_LINE)),
                (USER_SWITCH_HINTS[archive], flags(TITLE_HOME_LINE)),
                *((text, flags(STOCK_DISCONNECT_LINE)) for text in storage_notice_lines(archive)),
            ),
            # Preserve the original entry's zero padding and flags. Only the
            # one button code unit changes; do not re-encode the displayed string.
            # 保留原条目的零填充和标志，只改 L 字形，不重新编码整条可见字符串。
            replacement_values={TITLE_LANGUAGE_LINE: [
                ord(L_BUTTON) if unit == ord("L") else unit
                for unit in message_codec.read_message_values(original)[TITLE_LANGUAGE_LINE][0]
            ]},
        )
        title_entry = entries[GAME_TITLE_FILE_INDEX]
        original_titles = message_codec.read_message_values(title_entry.files[0])
        if len(original_titles) != GAME_TITLE_MESSAGE_BASE:
            raise ValueError(f"unexpected stock game-title count for {archive}: {len(original_titles)}")
        title_entry.files[0] = message_codec.patch_message_file(
            title_entry.files[0], {}, appended_value_lines=game_titles,
        )
        rebuilt = message_codec.write_garc(version, alignment, entries)
        rebuilt_entries = message_codec.read_garc(rebuilt)[2]
        rebuilt_lines = message_codec.read_message_lines(
            rebuilt_entries[MESSAGE_FILE_INDEX].files[0]
        )
        rebuilt_titles = message_codec.read_message_values(
            rebuilt_entries[GAME_TITLE_FILE_INDEX].files[0]
        )
        if rebuilt_titles != original_titles + list(game_titles):
            raise ValueError(f"game-title verification failed for {archive}")
        expected = {
            TITLE_HOME_LINE: original_lines[TITLE_HOME_LINE],
            TITLE_LANGUAGE_LINE: language_hint(original_lines[TITLE_LANGUAGE_LINE]),
            TITLE_OFFLINE_LINE: offline_title,
            TITLE_ONLINE_LINE: online_title,
            TITLE_SWITCH_LINE: TITLE_SWITCH_MESSAGES[archive],
            INITIAL_CONNECT_LINE: INTERNET_MESSAGES[archive],
            BANK_CONNECT_LINE: BANK_CONNECTION_MESSAGES[archive],
            SAVE_LINE: SAVE_MESSAGES[archive],
            DISCONNECT_LINE: DISCONNECT_MESSAGES[archive],
            MISSING_BANK_DISCONNECT_LINE: MISSING_BANK_DISCONNECT_MESSAGES[archive],
        }
        for line, value in expected.items():
            if rebuilt_lines[line] != value:
                raise ValueError(f"message verification failed for {archive}, line {line}")
        for table in (BANK_CONNECTION_MESSAGES,SAVE_MESSAGES):
            if set(table)!=set(ARCHIVES) or not 1<=len(table[archive].splitlines())<=3:
                raise ValueError(f"local-server text must cover every language within three rows: {archive}")
        notice=MISSING_BANK_DISCONNECT_MESSAGES[archive]
        if len(notice.splitlines())!=3 or notice.splitlines()[-1]!=DISCONNECT_MESSAGES[archive] or \
                max(len(line.encode("utf-16le"))//2 for line in notice.splitlines())>48:
            raise ValueError(f"missing-Bank disconnect text exceeds three lines/48 units in {archive}")
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(rebuilt)
        print(f"patched {archive} -> {destination}")

    build_title_archive(args.source_romfs, args.output_romfs, patch_label=label_from_arguments(args))
    build_title_fonts(args.source_romfs, args.output_romfs)
    verify_storage_notice_fonts((args.output_romfs / "a/0/2/3").read_bytes())


if __name__ == "__main__":
    main()
