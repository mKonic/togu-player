.pragma library
// Migaku's comprehension ratings.
function label(c) {
    if (c >= 99) return "Exceptional"
    if (c >= 95) return "Excellent"
    if (c >= 90) return "Great"
    if (c >= 80) return "Good"
    if (c >= 70) return "Approachable"
    if (c >= 60) return "Challenging"
    return "Ambitious"
}
function tone(c) {
    if (c >= 90) return "teal"
    if (c >= 80) return "blue"
    if (c >= 70) return "purple"
    if (c >= 60) return "orange"
    return "pink"
}
