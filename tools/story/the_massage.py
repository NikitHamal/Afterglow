"""The Massage - a couples' spa day for her birthday; the therapist leaves them alone with the oils."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from storylib import N, V, M, node, beat, C, emit

S = "the_massage"
story = {
    "id": S, "title": "The Massage",
    "tagline": "A birthday spa day for two. Then the therapist leaves them alone with the warm oil.",
    "setting": "A HOTEL SPA · AN AFTERNOON FOR TWO",
    "mood": "candle",
    "cast": {"vivian": {"wardrobe": "sundress_bikini"}, "marcus": {"wardrobe": "linen_shirt_shorts"}},
    "vars": {"close": 0, "trust": 0, "bold": 0},
    "chapters": [
        {"id": "c1", "title": "The Booking", "start": "c1_lobby"},
        {"id": "c2", "title": "Steam", "start": "c2_steam"},
        {"id": "c3", "title": "The Lesson", "start": "c3_lesson"},
        {"id": "c4", "title": "Your Turn", "start": "c4_turn"},
        {"id": "c5", "title": "Do Not Disturb", "start": "c5_alone"},
        {"id": "c6", "title": "Warm Oil", "start": "c6_oil"},
        {"id": "c7", "title": "Tea and Robes", "start": "c7_tea"},
    ],
    "gallery": {"g_hands": "His hands", "g_oil": "Warm oil", "g_tea": "Tea and robes"},
    "nodes": {},
}
n = story["nodes"]
SPA = dict(place="spa_suite", location="the private treatment suite", mood="candle", ambience="spa")

# ---------------------------------------------------------------- 1 The Booking
n["c1_lobby"] = node("c1", [
    N("The spa at the Hotel Ilios smells of eucalyptus and warm stone. Somewhere a fountain trickles. Everyone speaks in the kind of voice you use in churches and libraries."),
    N("Vivian has had the worst quarter of her career. Marcus has noticed, and has said nothing, and has quietly booked this: a couples' afternoon for her birthday, the full ritual, three hours."),
    V("Three hours? Marcus, I have forty emails waiting.", gesture="open", expr={"pout": 0.3}),
    M("You have forty emails and one birthday. Phone in the locker. That's the deal.", delivery="playful", gesture="point"),
    V("You're very bossy for a man in linen.", delivery="playful", expr={"smirk": 0.5}),
    M("The linen gives me courage.", react="laugh", gesture="shrug"),
], place="spa_lobby", location="the spa at the Hotel Ilios", mood="day", ambience="spa",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"}, pose="stand",
   choices=[
    C("Hand him your phone. \"Fine. I'm yours.\"", "c1_yield", trust="+1"),
    C("\"One email. Then I'm yours.\"", "c1_one"),
   ])
n["c1_yield"] = node("c1", [
    V("Fine. Take it. I'm yours for three hours.", delivery="soft", action="step_in", expr={"smile": 0.4}),
    M("Only three?", delivery="playful", react="smile"),
    N("He kisses her temple, and something in her shoulders that has been clenched since March lets go a quarter of an inch."),
], next="c2_steam")
n["c1_one"] = node("c1", [
    V("One email. Just one, and then I'm yours.", gesture="explain"),
    M("Vivian.", delivery="soft", head="tilt"),
    V("Fine. Fine! Zero emails.", react=None, expr={"smile": 0.5}, gesture="shrug"),
    N("She hands it over with the face of a woman surrendering a sword. He pockets it with the face of a man who has won a war."),
], next="c2_steam", set={"bold": "+1"})

# ---------------------------------------------------------------- 2 Steam
n["c2_steam"] = node("c2", [
    N("The steam room is tiled in blue and so thick with heat that the world ends a metre away. They sit side by side on the warm bench in their swimwear, sweating, silly with it."),
    V("I can't see you. Are you even still there?", delivery="playful"),
    M("I'm here. I'm melting, but I'm here.", react="laugh"),
    N("Her hand finds his knee in the fog. It stays there. His thumb traces the back of her fingers, idly, the way you'd touch something you're sure of."),
    M("When did you last take a whole day? For you?", delivery="soft"),
    V("I genuinely can't remember. Is that bad?", delivery="soft", look="down"),
], place="spa_steam", location="the steam room", mood="candle", ambience="spa", pose="sit",
   outfit={"vivian": "partial", "marcus": "partial"},
   choices=[
    C("\"Thank you for noticing. Nobody else did.\"", "c2_thanks", close="+1", trust="+1"),
    C("\"Is that your hand on my thigh, Mr Hale?\"", "c2_tease", bold="+1"),
   ])
n["c2_thanks"] = node("c2", [
    V("Thank you for noticing. That I wasn't all right. Nobody else did.", delivery="tender", look="partner"),
    M("I always notice. I just don't always know what to do about it. Today I knew.", delivery="soft", gesture="chest"),
], next="c3_lesson")
n["c2_tease"] = node("c2", [
    V("Is that your hand on my thigh, Mr Hale? In a public steam room?", delivery="playful", expr={"smirk": 0.6}),
    M("It's very foggy. It could be anyone's hand.", delivery="playful", react="laugh"),
    V("Then whoever it is, tell him not to stop.", delivery="whisper", expr={"bite_lip": 0.4}),
], next="c3_lesson")

# ---------------------------------------------------------------- 3 The Lesson
n["c3_lesson"] = node("c3", [
    N("The treatment suite is warm and dim, candles in glass along the walls, a wide padded bed covered in white towels. The therapist, Ana, has a voice like a slow river."),
    N("Today is a lesson, she explains. She will show them, and then they will do it for each other. Warm the oil in your hands first. Never take your hands away all at once. Breathe when they breathe."),
    N("Vivian lies down first. Marcus warms the oil between his palms, and lays his hands on her shoulders, and she makes a small, shocked sound that she will deny later."),
    M("Too much?", delivery="soft"),
    V("No. Don't you dare stop.", delivery="whisper", expr={"bliss": 0.5}),
    N("He works the knots out of her shoulders, slowly, his thumbs following the long muscles down either side of her spine. Ana murmurs approval and corrections. After a while, Vivian stops hearing her."),
], **SPA, pose="sit", outfit={"vivian": "partial", "marcus": "partial"}, gallery="g_hands",
   choices=[
    C("Melt. Let him take his time.", "c3_melt", close="+1"),
    C("Catch his eye, and smile in a way that is not about shoulders.", "c3_look", bold="+1"),
   ])
n["c3_melt"] = node("c3", [
    N("She melts. Every stroke of his hands takes a little of the year away with it. When he stops, she realises she has been nearly asleep and perfectly awake at the same time."),
    V("Where did you learn to do that?", delivery="soft", expr={"bliss": 0.3}),
    M("Thirty seconds ago. From Ana.", delivery="playful", react="laugh"),
], next="c4_turn")
n["c3_look"] = node("c3", [
    N("She turns her head on the towel and catches his eye, and smiles, slow, in a way that has absolutely nothing to do with her shoulders."),
    M("Ana's still here, Viv.", delivery="whisper", look="away", react="shy"),
    V("I know. Isn't it awful?", delivery="whisper", expr={"smirk": 0.6}),
], next="c4_turn")

# ---------------------------------------------------------------- 4 Your Turn
n["c4_turn"] = node("c4", [
    N("They swap. He lies down, broad back to the candlelight, and she warms the oil the way she was taught and lays her hands on him."),
    V("You're so tense. You've been carrying me all year, haven't you.", delivery="soft"),
    M("Somebody had to. You were carrying everything else.", delivery="soft", look="down"),
    N("She works down his back with the heels of her hands. He groans, low, and doesn't apologise for it. Ana smiles, and gathers her things, and says the most beautiful sentence in the language."),
    N("The suite is yours for the next hour. Nobody will disturb you."),
], **SPA, pose="sit", outfit={"vivian": "partial", "marcus": "partial"}, next="c5_alone")

# ---------------------------------------------------------------- 5 Do Not Disturb
n["c5_alone"] = node("c5", [
    N("The door clicks shut. The fountain trickles. The candles lean in their glasses."),
    V("An hour.", delivery="whisper", expr={"bite_lip": 0.4}),
    M("An hour.", delivery="whisper"),
    N("She doesn't move her hands from his back. Then she does, slowly, down his sides, oil-slick and warm, and he turns over beneath her.", action="step_in"),
], **SPA, pose="sit", outfit={"vivian": "partial", "marcus": "partial"},
   choices=[
    C("Kiss him.", "c5_kiss", close="+1"),
    C("\"Untie me.\" Turn your back to him.", "c5_untie", "bold >= 1 || close >= 2", bold="+1"),
    C("\"We're in a spa, Marcus.\" Laugh.", "c5_laugh"),
   ])
n["c5_kiss"] = node("c5", [
    N("She kisses him, and he tastes of eucalyptus and salt. His oiled hands slide up her back and find the knot of her bikini and pause there, asking.", action="kiss"),
    V("Yes.", delivery="whisper"),
], next="c6_oil")
n["c5_untie"] = node("c5", [
    V("Untie me.", delivery="whisper", look="away", expr={"smirk": 0.4}),
    N("She turns her back to him and lifts her hair. His fingers are slow on the knot at her neck. The string slides loose. She hears him breathe out, and smiles.", action="undress:vivian:under"),
], next="c6_oil")
n["c5_laugh"] = node("c5", [
    V("We're in a spa, Marcus. There's a woman called Ana just down the hall.", react=None, expr={"smile": 0.6}, gesture="open"),
    M("Ana said nobody would disturb us. I trust Ana completely.", delivery="playful", react="laugh"),
    V("You're terrible. Kiss me.", delivery="playful", action="kiss"),
], next="c6_oil")

# ---------------------------------------------------------------- 6 Warm Oil
n["c6_oil"] = node("c6", [
    N("What's left of the swimwear ends up on the floor with the towels. Everything is warm, and slick, and slow. Ana was right. Never take your hands away all at once.", action="undress:both:all"),
    N("He pours oil into his palm and lays her down on her front and kisses the length of her spine, and his hand slides down between her thighs, and she pushes back against it with a long, shaking breath.", action="to_bed"),
    V("Marcus. Please. Now.", delivery="breathless", expr={"pleasure": 0.6}, vox="moan02"),
    N("He covers her body with his, his weight warm along her back, and slides into her slowly, both of them gasping at how easy it is."),
], **SPA, pose="sit", outfit={"vivian": "partial", "marcus": "partial"}, act="prone", actT=0.2, gallery="g_oil",
   play={"act": "prone", "goal": "together", "next": "c7_tea", "acts": ["prone", "doggy", "spoon", "missionary"],
         "beats": {"start": "c6_b_start", "desire": "c6_b_desire", "edge": "c6_b_edge", "her_peak": "c6_b_peak"}})
n["c6_b_start"] = beat([V("Slowly. Like Ana said.", delivery="breathless")])
n["c6_b_desire"] = beat([V("Your hands. Don't take your hands away.", delivery="breathless")])
n["c6_b_edge"] = beat([V("Not yet. Breathe with me.", delivery="whisper")])
n["c6_b_peak"] = beat([V("Oh... oh my god.", delivery="breathless"), M("Happy birthday.", delivery="whisper")])

# ---------------------------------------------------------------- 7 Tea and Robes
n["c7_tea"] = node("c7", [
    N("Later, much later, they sit side by side in the relaxation room in white towels, with mint tea neither of them is drinking, pink and dazed and helplessly pleased with themselves."),
    V("I think we broke at least four spa rules.", delivery="playful", expr={"smile": 0.6}),
    M("Five. I counted.", react="laugh", gesture="shrug"),
    V("Thank you. For today. For noticing.", delivery="tender", look="partner"),
], place="spa_lounge", location="the relaxation room", mood="day", ambience="spa", pose="sit",
   outfit={"vivian": "partial", "marcus": "partial"}, gallery="g_tea",
   choices=[
    C("\"Every month. This. Promise me.\"", "c7_ritual", "close >= 2"),
    C("\"Move in with me. I'm tired of saying goodnight at the door.\"", "c7_ask", "trust >= 2 || bold >= 2"),
    C("\"Best birthday in years.\" Rest your head on his shoulder.", "c7_lovely"),
   ])
n["c7_ritual"] = node("c7", [
    V("Every month. This. Not the spa. Us, a whole day, phones in a locker. Promise me.", gesture="explain", delivery="soft"),
    M("Every month. I'll book it before you can open your laptop.", head="nod", react="smile"),
    N("They keep it. Some months it's a spa, some months it's a picnic, once it's a whole day in bed. The phones always go in a drawer."),
], end={"title": "The Ritual", "text": "Love needs looking after, like anything that matters. They decided to look after it on purpose."})
n["c7_ask"] = node("c7", [
    V("Move in with me. I'm tired of saying goodnight at your door.", delivery="soft", look="down", expr={"shy": 0.4}),
    M("Are you asking me that because of the massage?", delivery="playful", head="tilt"),
    V("I'm asking because of the noticing. The massage just helped.", expr={"smile": 0.6}, react=None),
    M("Yes. Obviously yes.", delivery="tender", action="kiss"),
], end={"title": "Something to Ask", "text": "She went in for a massage and came out with a co-signer on her lease. Happy birthday, Vivian."})
n["c7_lovely"] = node("c7", [
    V("Best birthday in years.", delivery="soft", expr={"bliss": 0.3}),
    M("Best birthday you've had with me, at least.", delivery="playful"),
    V("That's what I said.", delivery="soft", react="laugh"),
    N("She falls asleep on his shoulder in the relaxation room, and the staff are kind enough to let her."),
], end={"title": "A Lovely Day", "text": "Some days don't change your life. They just remind you what it's for."})

if __name__ == "__main__":
    sys.exit(0 if emit(story, sys.argv[1]) else 1)
