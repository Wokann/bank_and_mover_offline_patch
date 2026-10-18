"""Independent single-line mode labels and R hints for Bank and Mover."""

L_BUTTON = "\ue004"
R_BUTTON = "\ue005"
USER_LABEL_MESSAGES = {
    "0/0/4": "現在のユーザー：", "0/0/5": "現在のユーザー：",
    "0/0/6": "Current user: ", "0/0/7": "Utilisateur : ",
    "0/0/8": "Utente: ", "0/0/9": "Benutzer: ",
    "0/1/0": "Usuario: ", "0/1/1": "현재 사용자: ",
    "0/1/2": "当前用户：", "0/1/3": "目前用戶：",
}
USER_SWITCH_HINTS = {
    "0/0/4": "（↑/↓で切替）", "0/0/5": "（↑/↓で切替）",
    "0/0/6": " (↑/↓ to switch)", "0/0/7": " (↑/↓ : changer)",
    "0/0/8": " (↑/↓ per cambiare)", "0/0/9": " (↑/↓ wechseln)",
    "0/1/0": " (↑/↓ para cambiar)", "0/1/1": " (↑/↓ 전환)",
    "0/1/2": "（按↑/↓键切换用户）", "0/1/3": "（按↑/↓鍵切換用戶）",
}
MODE_LABELS = {
    "0/0/4": "現在のモード：",
    "0/0/5": "現在のモード：",
    "0/0/6": "Current mode: ",
    "0/0/7": "Mode actuel : ",
    "0/0/8": "Modalità attuale: ",
    "0/0/9": "Aktueller Modus: ",
    "0/1/0": "Modo actual: ",
    "0/1/1": "현재 모드: ",
    "0/1/2": "当前模式：",
    "0/1/3": "目前模式：",
}
MODE_SWITCH_HINTS = {
    "0/0/4": f"{R_BUTTON}ボタンを押すとモードを切り替えられます",
    "0/0/5": f"{R_BUTTON}ボタンを押すとモードを切り替えられます",
    "0/0/6": f"Press the {R_BUTTON} Button to change mode",
    "0/0/7": f"Appuyez sur le bouton {R_BUTTON} pour changer de mode.",
    "0/0/8": f"Premi il pulsante {R_BUTTON} per cambiare modalità.",
    "0/0/9": f"Drücke die {R_BUTTON}-Taste, um den Modus zu ändern.",
    "0/1/0": f"Pulsa el botón {R_BUTTON} para cambiar de modo.",
    "0/1/1": f"{R_BUTTON} 버튼을 누르면 모드를 전환할 수 있습니다",
    "0/1/2": f"如果按{R_BUTTON}键，就可以切换模式。",
    "0/1/3": f"按{R_BUTTON}鍵可切換模式",
}
MODE_NAMES = {
    "0/0/4": ("オフライン", "ダウンロード", "アンロック", "オリジナル", "オンライン"),
    "0/0/5": ("オフライン", "ダウンロード", "アンロック", "オリジナル", "オンライン"),
    "0/0/6": ("Offline", "Download", "Unlock", "Original", "Online"),
    "0/0/7": ("Hors ligne", "Téléchargement", "Déverrouillage", "Original", "En ligne"),
    "0/0/8": ("Offline", "Download", "Sblocco", "Originale", "Online"),
    "0/0/9": ("Offline", "Download", "Entsperren", "Original", "Online"),
    "0/1/0": ("Sin conexión", "Descarga", "Desbloqueo", "Original", "En línea"),
    "0/1/1": ("오프라인", "다운로드", "잠금 해제", "원본", "온라인"),
    "0/1/2": ("离线模式", "下载模式", "解锁模式", "原版模式", "在线模式"),
    "0/1/3": ("離線模式", "下載模式", "解鎖模式", "原版模式", "在線模式"),
}


def language_hint(text: str) -> str:
    """Preserve native wording; replace its one literal L with the button glyph."""
    if text.count("L") != 1:
        raise ValueError("native language hint must contain exactly one L button label")
    return text.replace("L", L_BUTTON)


def mode_messages(mode: str) -> dict[str, str]:
    index = ("offline", "download", "unlock", "original", "online").index(mode)
    result = {archive: f"{label}{MODE_NAMES[archive][index]}"
              for archive, label in MODE_LABELS.items()}
    for archive, text in result.items():
        if len(text.splitlines()) != 1 or len(text.encode("utf-16le")) > 254:
            raise ValueError(f"invalid single-line mode label: {archive} {mode}")
    return result


def mode_switch_hint(archive: str) -> str:
    text = MODE_SWITCH_HINTS[archive]
    if len(text.splitlines()) != 1 or text.count(R_BUTTON) != 1 or len(text.encode("utf-16le")) > 108:
        raise ValueError(f"invalid single-line R-button hint: {archive}")
    return text
