import QtQuick
import QtQuick.Layouts

ColumnLayout {
    spacing: 16
    Section {
        title: qsTr("Titles and covers")
        SwitchRow { label: qsTr("Look up series online"); hint: qsTr("Cover art, titles and descriptions from AniList (no account needed). Off: folder images and file names only."); key: "metadata" }
        SettingRow {
            label: qsTr("TMDB API key")
            hint: qsTr("Optional, for live-action shows and films. Free from themoviedb.org → Settings → API.")
            TField { width: 300; text: Settings.tmdbKey; echoMode: TextInput.Password; onEditingFinished: Settings.set("tmdbKey", text.trim()) }
        }
    }
    Section {
        title: qsTr("Jimaku")
        SettingRow {
            label: qsTr("API key")
            hint: qsTr("For finding Japanese subtitles on jimaku.cc. Free with an account: jimaku.cc → Account → API key.")
            TField { width: 300; text: Settings.jimakuKey; echoMode: TextInput.Password; onEditingFinished: Settings.set("jimakuKey", text.trim()) }
        }
    }
}
