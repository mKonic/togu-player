pragma Singleton
import QtQuick

// Icon shapes on a 24-unit grid, drawn with round 2-unit strokes unless noted.
QtObject {
    readonly property var paths: ({
        "play": "<path d='M7 4.5v15l12.5-7.5z' fill='C' stroke='C' stroke-linejoin='round'/>",
        "pause": "<rect x='6' y='4.5' width='4' height='15' rx='1' fill='C'/><rect x='14' y='4.5' width='4' height='15' rx='1' fill='C'/>",
        "back": "<path d='M15 5l-7 7 7 7'/>",
        "forward": "<path d='M9 5l7 7-7 7'/>",
        "down": "<path d='M5 9l7 7 7-7'/>",
        "up": "<path d='M5 15l7-7 7 7'/>",
        "close": "<path d='M6 6l12 12M18 6L6 18'/>",
        "prevLine": "<path d='M18 6l-7 6 7 6z' fill='C'/><path d='M7 5v14'/>",
        "nextLine": "<path d='M6 6l7 6-7 6z' fill='C'/><path d='M17 5v14'/>",
        "replay": "<path d='M4 12a8 8 0 1 0 2.5-5.8'/><path d='M4 4v4.5h4.5'/>",
        "seekBack": "<path d='M11 7l-6 5 6 5z' fill='C'/><path d='M19 7l-6 5 6 5z' fill='C'/>",
        "seekFwd": "<path d='M5 7l6 5-6 5z' fill='C'/><path d='M13 7l6 5-6 5z' fill='C'/>",
        "subtitles": "<rect x='3' y='5' width='18' height='14' rx='3'/><path d='M7 12h3M13 12h4M7 15.5h6M15.5 15.5h1.5'/>",
        "list": "<path d='M9 6h11M9 12h11M9 18h11'/><circle cx='4.5' cy='6' r='1' fill='C'/><circle cx='4.5' cy='12' r='1' fill='C'/><circle cx='4.5' cy='18' r='1' fill='C'/>",
        "settings": "<circle cx='12' cy='12' r='3'/><path d='M12 2.5v3M12 18.5v3M2.5 12h3M18.5 12h3M5.3 5.3l2.1 2.1M16.6 16.6l2.1 2.1M5.3 18.7l2.1-2.1M16.6 7.4l2.1-2.1'/>",
        "volume": "<path d='M4 9.5h3.5L12 5.5v13l-4.5-4H4z' fill='C'/><path d='M15.5 9a4 4 0 0 1 0 6M18 6.5a7.5 7.5 0 0 1 0 11'/>",
        "mute": "<path d='M4 9.5h3.5L12 5.5v13l-4.5-4H4z' fill='C'/><path d='M16 9.5l5 5M21 9.5l-5 5'/>",
        "fullscreen": "<path d='M4 9V4h5M15 4h5v5M20 15v5h-5M9 20H4v-5'/>",
        "unfullscreen": "<path d='M9 4v5H4M20 9h-5V4M15 20v-5h5M4 15h5v5'/>",
        "search": "<circle cx='11' cy='11' r='6.5'/><path d='M16 16l4.5 4.5'/>",
        "folder": "<path d='M3.5 7.5A2 2 0 0 1 5.5 5.5h4l2 2h7a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2h-13a2 2 0 0 1-2-2z'/>",
        "plus": "<path d='M12 5v14M5 12h14'/>",
        "minus": "<path d='M5 12h14'/>",
        "film": "<rect x='3.5' y='4' width='17' height='16' rx='2.5'/><path d='M3.5 9h17M3.5 15h17M8.5 4v16M15.5 4v16'/>",
        "music": "<path d='M9 18V6l11-2v12'/><circle cx='6.5' cy='18' r='2.5'/><circle cx='17.5' cy='16' r='2.5'/>",
        "book": "<path d='M4 5.5A1.5 1.5 0 0 1 5.5 4H11v15.5H5.5A1.5 1.5 0 0 0 4 21z'/><path d='M20 5.5A1.5 1.5 0 0 0 18.5 4H13v15.5h5.5A1.5 1.5 0 0 1 20 21z'/>",
        "manga": "<rect x='3.5' y='3.5' width='17' height='17' rx='2'/><path d='M3.5 11h9M12.5 3.5v17M12.5 14.5h8'/>",
        "stats": "<path d='M5 20V11M12 20V5M19 20v-7'/>",
        "words": "<path d='M4 6h16M4 12h10M4 18h7'/><path d='M16 15l2 2 4-4'/>",
        "card": "<rect x='3' y='5' width='14' height='15' rx='2.5'/><path d='M7 5V4a1.5 1.5 0 0 1 1.5-1.5h11A1.5 1.5 0 0 1 21 4v13a1.5 1.5 0 0 1-1.5 1.5H17'/>",
        "cardAdd": "<rect x='3' y='5' width='14' height='15' rx='2.5'/><path d='M10 9v7M6.5 12.5h7'/><path d='M7 5V4a1.5 1.5 0 0 1 1.5-1.5h11A1.5 1.5 0 0 1 21 4v13a1.5 1.5 0 0 1-1.5 1.5H17'/>",
        "check": "<path d='M4.5 12.5l5 5L19.5 7'/>",
        "star": "<path d='M12 2.5l2.9 6 6.6.9-4.8 4.6 1.2 6.5L12 17.4l-5.9 3.1 1.2-6.5L2.5 9.4l6.6-.9z' fill='C' stroke='none'/>",
        "paw": "<ellipse cx='12' cy='16' rx='6' ry='5' fill='C' stroke='none'/><circle cx='4' cy='11' r='2.4' fill='C' stroke='none'/><circle cx='8.5' cy='5.5' r='2.4' fill='C' stroke='none'/><circle cx='15.5' cy='5.5' r='2.4' fill='C' stroke='none'/><circle cx='20' cy='11' r='2.4' fill='C' stroke='none'/>",
        "sync": "<path d='M20 12a8 8 0 0 1-14 5.3M4 12a8 8 0 0 1 14-5.3'/><path d='M18 3v4h-4M6 21v-4h4'/>",
        "bookmark": "<path d='M6.5 3.5h11v17l-5.5-4-5.5 4z'/>",
        "bookmarkOn": "<path d='M6.5 3.5h11v17l-5.5-4-5.5 4z' fill='C'/>",
        "more": "<circle cx='5.5' cy='12' r='1.5' fill='C'/><circle cx='12' cy='12' r='1.5' fill='C'/><circle cx='18.5' cy='12' r='1.5' fill='C'/>",
        "trash": "<path d='M4.5 6.5h15M9.5 6.5V4.5h5v2M6.5 6.5l1 13.5h9l1-13.5'/>",
        "eye": "<path d='M2.5 12s3.5-6.5 9.5-6.5 9.5 6.5 9.5 6.5-3.5 6.5-9.5 6.5S2.5 12 2.5 12z'/><circle cx='12' cy='12' r='3'/>",
        "eyeOff": "<path d='M3 3l18 18M10.5 5.6A9.6 9.6 0 0 1 12 5.5c6 0 9.5 6.5 9.5 6.5a17 17 0 0 1-2.6 3.4M6.6 6.6A16 16 0 0 0 2.5 12s3.5 6.5 9.5 6.5a9 9 0 0 0 4.6-1.3'/>",
        "clock": "<circle cx='12' cy='12' r='8.5'/><path d='M12 7.5V12l3 2'/>",
        "image": "<rect x='3.5' y='4.5' width='17' height='15' rx='2.5'/><circle cx='9' cy='10' r='1.8'/><path d='M20.5 16l-5-5-9 8.5'/>",
        "mic": "<rect x='9' y='3' width='6' height='11' rx='3'/><path d='M5.5 11a6.5 6.5 0 0 0 13 0M12 17.5V21'/>",
        "scissors": "<circle cx='6' cy='6.5' r='2.5'/><circle cx='6' cy='17.5' r='2.5'/><path d='M8 8l12 10M8 16L20 6'/>",
        "queue": "<path d='M4 6h12M4 12h12M4 18h7'/><path d='M18 15v6M15 18h6'/>",
        "refresh": "<path d='M20 11a8 8 0 1 0-2.3 5.7'/><path d='M20 4v7h-7'/>",
        "external": "<path d='M14 4h6v6M20 4l-9 9'/><path d='M18 14v4.5a1.5 1.5 0 0 1-1.5 1.5h-11A1.5 1.5 0 0 1 4 18.5v-11A1.5 1.5 0 0 1 5.5 6H10'/>",
        "home": "<path d='M4 11l8-7 8 7v8.5a1 1 0 0 1-1 1h-4.5V15h-5v5.5H5a1 1 0 0 1-1-1z'/>",
        "speed": "<path d='M4.5 17a8.5 8.5 0 1 1 15 0'/><path d='M12 13l4-5'/>",
        "translate": "<path d='M4 6h9M8.5 4v2M6 6c.5 3 2.5 5.5 5 7M11 6c-.5 3-3 6.5-6.5 8'/><path d='M13 20l3.5-9 3.5 9M14.2 17h4.6'/>",
        "keyboard": "<rect x='2.5' y='6' width='19' height='12' rx='2'/><path d='M6 10h1M10 10h1M14 10h1M18 10h0M7 14h10'/>",
        "info": "<circle cx='12' cy='12' r='8.5'/><path d='M12 11v5.5M12 7.5v.5'/>",
        "warning": "<path d='M12 3.5l9.5 16.5h-19z'/><path d='M12 10v4.5M12 17.5v.5'/>",
        "grid": "<rect x='4' y='4' width='6.5' height='6.5' rx='1.5'/><rect x='13.5' y='4' width='6.5' height='6.5' rx='1.5'/><rect x='4' y='13.5' width='6.5' height='6.5' rx='1.5'/><rect x='13.5' y='13.5' width='6.5' height='6.5' rx='1.5'/>",
        "download": "<path d='M12 4v11M7 10.5l5 5 5-5M5 19.5h14'/>",
        "upload": "<path d='M12 20V9M7 13.5l5-5 5 5M5 4.5h14'/>",
        "audio": "<path d='M3 12h2M7 8v8M11 5v14M15 9v6M19 7v10M21 12h0'/>",
        "user": "<circle cx='12' cy='8' r='4'/><path d='M4.5 20.5a7.5 7.5 0 0 1 15 0'/>",
        "edit": "<path d='M4 20h4L19 9l-4-4L4 16z'/><path d='M13.5 6.5l4 4'/>",
        "layers": "<path d='M12 3.5l9 5-9 5-9-5z'/><path d='M3 13l9 5 9-5'/>",
        "flame": "<path d='M12 21c-4 0-6.5-2.8-6.5-6.2 0-4.3 4-6.2 4.5-10.3 3 2 4 4.5 4 6.5 1-.5 1.8-1.7 2-3 1.6 1.8 2.5 4 2.5 6.6 0 3.6-2.7 6.4-6.5 6.4z'/>",
        "target": "<circle cx='12' cy='12' r='8.5'/><circle cx='12' cy='12' r='4.5'/><circle cx='12' cy='12' r='1' fill='C'/>"
    })

    function svg(name, color, weight) {
        var body = paths[name] || paths["info"]
        var c = String(color)
        return "data:image/svg+xml;utf8," + encodeURIComponent(
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='" + c +
            "' stroke-width='" + (weight || 2) + "' stroke-linecap='round' stroke-linejoin='round'>" +
            body.replace(/='C'/g, "='" + c + "'") + "</svg>")
    }
}
