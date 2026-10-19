"""Localized, acknowledged local-storage failures (I/O, space, capacity query)."""

STORAGE_NOTICE_MESSAGES = {
    "0/0/4": (
        "SDカードの　よみかきに　しっぱいしました。\nせつぞくと　ロックを　かくにんしてください。\nAボタンで　しゅうりょうします。",
        "SDカードの　あきようりょうが　たりません。\nおえてから　あきようりょうを　ふやしてください。\nAボタンで　しゅうりょうします。",
        "SDカードの　あきようりょうが　ふめいです。\nおえてから　カードを　かくにんしてください。\nAボタンで　しゅうりょうします。",
    ),
    "0/0/5": (
        "SDカードの読み書きに失敗しました。\n接続や書き込みロックを確認してください。\nAボタンで終了します。",
        "SDカードの空き容量が足りません。\nソフトを終了してから空き容量を増やしてください。\nAボタンで終了します。",
        "SDカードの空き容量を確認できません。\nソフトを終了してからカードを確認してください。\nAボタンで終了します。",
    ),
    "0/0/6": (
        "Could not read or write the SD card.\nCheck its connection and write protection.\nPress A to exit.",
        "Not enough free space on the SD card.\nFree some space after closing the software.\nPress A to exit.",
        "Could not check free space on the SD card.\nCheck the card after closing the software.\nPress A to exit.",
    ),
    "0/0/7": (
        "Échec de lecture/écriture sur la carte SD.\nVérifiez la connexion et le verrouillage.\nAppuyez sur A pour quitter.",
        "Espace insuffisant sur la carte SD.\nLibérez de l’espace après avoir quitté.\nAppuyez sur A pour quitter.",
        "Espace libre de la carte SD inaccessible.\nVérifiez la carte après avoir quitté.\nAppuyez sur A pour quitter.",
    ),
    "0/0/8": (
        "Lettura o scrittura della scheda SD fallita.\nVerifica collegamento e blocco scrittura.\nPremi A per uscire.",
        "Spazio libero sulla scheda SD insufficiente.\nLibera spazio dopo aver chiuso il software.\nPremi A per uscire.",
        "Impossibile verificare lo spazio libero SD.\nControlla la scheda dopo aver chiuso.\nPremi A per uscire.",
    ),
    "0/0/9": (
        "SD-Karte: Lesen/Schreiben fehlgeschlagen.\nPrüfe Verbindung und Schreibschutz.\nDrücke A zum Beenden.",
        "Auf der SD-Karte ist nicht genug Platz.\nSchaffe nach dem Beenden freien Speicher.\nDrücke A zum Beenden.",
        "Freier SD-Speicher konnte nicht geprüft werden.\nPrüfe die SD-Karte nach dem Beenden.\nDrücke A zum Beenden.",
    ),
    "0/1/0": (
        "Error al leer o escribir en la tarjeta SD.\nRevisa la conexión y el bloqueo de escritura.\nPulsa A para salir.",
        "No hay suficiente espacio en la tarjeta SD.\nLibera espacio después de cerrar el programa.\nPulsa A para salir.",
        "No se puede comprobar el espacio libre SD.\nRevisa la tarjeta tras cerrar el programa.\nPulsa A para salir.",
    ),
    "0/1/1": (
        "SD 카드 읽기 또는 쓰기에 실패했습니다.\n연결 상태와 쓰기 방지를 확인해 주십시오.\nA 버튼을 누르면 종료합니다.",
        "SD 카드의 빈 공간이 부족합니다.\n소프트웨어 종료 후 공간을 확보해 주십시오.\nA 버튼을 누르면 종료합니다.",
        "SD 카드의 빈 공간을 확인할 수 없습니다.\n소프트웨어 종료 후 카드를 확인해 주십시오.\nA 버튼을 누르면 종료합니다.",
    ),
    "0/1/2": (
        "无法读写 SD 卡。\n请检查连接状态、写保护等情况。\n按 A 键结束。",
        "SD 卡剩余空间不足。\n请结束软件后清理空间。\n按 A 键结束。",
        "无法查询 SD 卡剩余空间。\n请结束软件后检查 SD 卡。\n按 A 键结束。",
    ),
    "0/1/3": (
        "無法讀寫 SD 卡。\n請檢查連線狀態、寫入保護等情況。\n按 A 鍵結束。",
        "SD 卡剩餘空間不足。\n請結束軟體後清理空間。\n按 A 鍵結束。",
        "無法查詢 SD 卡剩餘空間。\n請結束軟體後檢查 SD 卡。\n按 A 鍵結束。",
    ),
}


def storage_notice_lines(archive):
    lines = STORAGE_NOTICE_MESSAGES[archive]
    for text in lines:
        rows = text.splitlines()
        if len(rows) != 3 or max(len(row.encode("utf-16le")) // 2 for row in rows) > 48:
            raise ValueError(f"SD notice exceeds three rows/48 units: {archive}")
    return lines


def verify_storage_notice_fonts(data):
    from title_font import character_widths
    from tools.message_archive import read_garc
    entries = read_garc(data)[2]
    fonts = [character_widths(entries[index].files[0]) for index in range(3)]
    for archive in STORAGE_NOTICE_MESSAGES:
        widths = fonts[1 if archive == "0/1/2" else 2 if archive == "0/1/3" else 0]
        for text in storage_notice_lines(archive):
            for row in text.splitlines():
                if any(char not in widths for char in row) or sum(widths[char] for char in row) > 360:
                    raise ValueError(f"SD notice has a missing glyph or exceeds 360px: {archive} {row!r}")
