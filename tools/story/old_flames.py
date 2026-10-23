"""Old Flames - twenty years after they split, two exes meet again at a friend's wedding by the sea."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from storylib import N, V, M, node, beat, C, emit

S = "old_flames"
story = {
    "id": S, "title": "Old Flames",
    "tagline": "Twenty years later, the same smile across a wedding reception.",
    "setting": "A COASTAL HOTEL · A WEDDING WEEKEND",
    "mood": "evening",
    "cast": {"vivian": {"wardrobe": "evening_dress"}, "marcus": {"wardrobe": "suit_open_collar"}},
    "vars": {"trust": 0, "warmth": 0, "honest": 0, "kissed": 0},
    "chapters": [
        {"id": "c1", "title": "The Toast", "start": "c1_reception"},
        {"id": "c2", "title": "The Terrace", "start": "c2_terrace"},
        {"id": "c3", "title": "The Last Dance", "start": "c3_dance"},
        {"id": "c4", "title": "Her Door", "start": "c4_door"},
        {"id": "c5", "title": "Rediscovery", "start": "c5_room"},
        {"id": "c6", "title": "After", "start": "c6_after"},
        {"id": "c7", "title": "Morning", "start": "c7_morning"},
    ],
    "gallery": {"g_door": "Her door", "g_first": "Rediscovery", "g_morning": "The morning after"},
    "nodes": {},
}
n = story["nodes"]

# ---------------------------------------------------------------- 1 The Toast
n["c1_reception"] = node("c1", [
    N("The ballroom of the Hotel Miramar glows like the inside of a seashell. Nina's wedding has reached the hour where the ties come loose and the shoes come off."),
    N("Vivian is at the bar with a glass of champagne she isn't drinking, because across the room, laughing at something the best man said, is Marcus Hale."),
    N("Grey at the temples now. Broader in the shoulders. The same way of listening with his whole face. He sees her a heartbeat later, and stops mid-sentence."),
    M("Vivian."),
    V("Marcus. You came all the way from Lisbon for a wedding?", gesture="open", expr={"smile": 0.35}),
    M("Nina threatened me. You know how she is. And I hoped...", gesture="explain"),
    M("I hoped you'd be here.", delivery="soft", look="down", head="down"),
    N("Twenty years ago they were twenty-two and certain of everything. Then a job offer, an airport, a promise to call that slowly turned into silence."),
], place="hotel_ballroom", location="the ballroom of the Hotel Miramar", mood="evening", ambience="party",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"}, pose="stand",
   choices=[
    C("\"I hoped so too. I've been staring at the door all night.\"", "c1_honest", trust="+1", honest="+1"),
    C("\"You still wear a tie like it's trying to escape.\"", "c1_tease", warmth="+1"),
    C("Keep it polite. \"It's good to see you, Marcus.\"", "c1_polite"),
   ])
n["c1_honest"] = node("c1", [
    V("I hoped so too. I've been watching the door all night like a teenager.", delivery="soft", expr={"shy": 0.4}, look="down"),
    M("You always did say the thing I was too scared to.", react=None, expr={"him.pleasure": 0.2}),
    N("Something old and warm settles between them, like a song you didn't know you still knew all the words to."),
], next="c2_terrace")
n["c1_tease"] = node("c1", [
    V("You still wear a tie like it's trying to escape.", delivery="playful", expr={"smirk": 0.6}, gesture="point"),
    M("It is trying to escape. Twenty years and it's never once succeeded.", react="laugh", gesture="shrug"),
    N("He laughs, the real laugh, the one she used to fish for across crowded rooms. For a moment the years fold up like a fan."),
], next="c2_terrace")
n["c1_polite"] = node("c1", [
    V("It's good to see you, Marcus. Really.", gesture="open"),
    M("It's good to see you too.", head="nod"),
    N("Polite. Careful. Two people handing each other a fragile thing and agreeing not to look at it too closely. Not yet."),
], next="c2_terrace")

# ---------------------------------------------------------------- 2 The Terrace
n["c2_terrace"] = node("c2", [
    N("The terrace hangs over the sea. Paper lanterns sway in the wind off the water, and the music from inside arrives softened, as if from another decade."),
    M("I have to ask. Are you happy, Viv?", delivery="soft"),
    V("Mostly. I teach. I have a flat with too many books and a balcony that catches the evening sun. I was married for nine years.", gesture="explain"),
    V("It ended kindly. That's the most I can say for it.", look="away", expr={"pout": 0.2}),
    M("I'm sorry.", react=None),
    V("Don't be. And you?", look="partner"),
    M("I built a life that looks very good in photographs.", gesture="shrug", head="tilt"),
    N("The sea turns over below them, patient, endless. He is close enough now that she can smell the cedar of his cologne, and under it, him."),
], place="hotel_terrace", location="the terrace above the sea", mood="candle", ambience="sea", pose="stand",
   choices=[
    C("\"Why did you never call?\"", "c2_why", honest="+1"),
    C("Take his hand on the railing.", "c2_hand", warmth="+1", trust="+1"),
    C("\"Let's not dig up the past tonight.\"", "c2_light"),
   ])
n["c2_why"] = node("c2", [
    V("Why did you never call? After Lisbon. I waited, you know. Longer than I'll admit.", delivery="soft", expr={"pout": 0.25}),
    M("Because every time I picked up the phone, I knew you'd hear it in my voice. That I wanted you to come. And I couldn't ask you to give up everything for me.", gesture="chest", delivery="tender"),
    V("You didn't ask. So I never got to say yes.", look="down", head="down"),
    N("The words land between them like a stone dropped into still water. The rings spread out across twenty years."),
], next="c3_dance")
n["c2_hand"] = node("c2", [
    N("She lays her hand over his on the cool stone railing. He turns his palm up to meet it without a word, the way he used to, as if no time had passed at all."),
    M("Your hands are cold.", delivery="soft", react="smile"),
    V("Then warm them.", delivery="playful", expr={"smile": 0.5}),
    N("He does. He holds both her hands between his and breathes on them, and she laughs, and the laugh catches somewhere low in her chest."),
], next="c3_dance")
n["c2_hand"]["lines"][0]["action"] = "step_in"
n["c2_light"] = node("c2", [
    V("Let's not dig up the past tonight. It's a wedding. We're supposed to be happy.", gesture="open"),
    M("Then let's be happy. I can manage that.", head="nod", expr={"him.pleasure": 0.1}),
    N("They talk about Nina's terrible speech, about the band, about nothing. It's easy. It was always easy. That was never the problem."),
], next="c3_dance")

# ---------------------------------------------------------------- 3 The Last Dance
n["c3_dance"] = node("c3", [
    N("Inside, the band slows down for the last song of the night. Couples drift onto the floor, older ones holding each other like they've done it a thousand times."),
    M("They're playing our song. Well. A song. I'm claiming it.", gesture="reach", delivery="playful"),
    V("We never had a song.", expr={"smirk": 0.4}),
    M("Then let's have one now. Dance with me, Vivian.", delivery="soft", head="tilt"),
], place="hotel_ballroom", location="the ballroom, the last song", mood="evening", ambience="party", pose="stand",
   choices=[
    C("Step into his arms.", "c3_yes", warmth="+1"),
    C("\"Walk me down to the beach instead.\"", "c3_beach", trust="+1"),
    C("\"It's late, Marcus. I should say goodnight.\"", "c3_night"),
   ])
n["c3_yes"] = node("c3", [
    N("She steps in. His hand finds the small of her back, bare above the low cut of the dress, and she feels him take a breath he has to remember to let go of.", action="embrace"),
    V("You still lead like you're apologising.", delivery="whisper", expr={"smile": 0.4}),
    M("I'm out of practice. Twenty years out of practice.", delivery="soft"),
    N("They sway. Her cheek rests near his jaw. The room, the band, the lanterns, all of it goes quiet around them, until it's only breath and the slow turn of the floor."),
    M("Can I walk you up?", delivery="whisper"),
], next="c4_door")
n["c3_beach"] = node("c3", [
    V("Walk me down to the beach instead. I want to take these shoes off.", gesture="open", delivery="playful"),
    N("They slip out past the band and down the stone steps to the sand, her shoes hanging from his fingers. The water is black and silver under the moon."),
    M("Do you remember the night on the pier? The storm?", delivery="soft", react="smile"),
    V("I remember you gave me your jacket and caught a cold for a week.", expr={"smile": 0.6}, react="laugh"),
    M("Best cold of my life.", head="nod"),
], next="c4_door", set={"trust": "+1"})
n["c3_night"] = node("c3", [
    V("It's late, Marcus. I should say goodnight.", look="down"),
    M("Of course. Let me at least walk you to your room. The corridors in this place are a labyrinth.", gesture="open"),
    N("She lets him. Some part of her had been hoping he'd ask."),
], next="c4_door")

# ---------------------------------------------------------------- 4 Her Door
n["c4_door"] = node("c4", [
    N("The corridor is hushed and carpeted, the noise of the party fading behind them with every door they pass. Room four-twelve. Her key card is in her hand. Neither of them moves."),
    M("Well. Here we are.", delivery="soft", gesture="shrug"),
    V("Here we are.", delivery="soft", expr={"bite_lip": 0.3}),
    N("He looks at her mouth, and then, carefully, back at her eyes. Asking. He was always asking."),
], place="hotel_corridor", location="the corridor outside room four-twelve", mood="evening", ambience="", pose="stand",
   choices=[
    C("Kiss him.", "c4_kiss", kissed="=1", warmth="+1"),
    C("\"Come in. Just to talk.\"", "c4_talk", trust="+1"),
    C("\"Goodnight, Marcus.\" Touch his cheek, and go in alone.", "c4_alone"),
   ])
n["c4_kiss"] = node("c4", [
    N("She closes the distance before she can think better of it. His lips are warm, surprised for one second, and then not surprised at all.", action="kiss", sfx=None),
    N("It isn't the kiss of two twenty-two year olds. It is slower. It knows what it's worth.", shot="kiss"),
    M("Vivian...", delivery="whisper", action="part", expr={"him.pleasure": 0.4}),
    V("Come inside.", delivery="whisper", expr={"bliss": 0.3, "smile": 0.3}),
], next="c5_room", gallery="g_door")
n["c4_talk"] = node("c4", [
    V("Come in. Just to talk. I'm not ready for tonight to be over.", gesture="open", delivery="soft"),
    M("Just to talk.", head="nod", react="smile"),
    N("They both know it's a lie, and they both let it stand, because it's a kind one."),
], next="c5_room")
n["c4_alone"] = node("c4", [
    N("She lays her palm against his cheek. He leans into it and closes his eyes, and for a moment they are both twenty-two."),
    V("Goodnight, Marcus.", delivery="tender"),
    M("Goodnight, Viv.", delivery="soft"),
    N("The door closes between them with a soft click. She stands on her side of it for a long time. She suspects he does too."),
], next="c7_friends")

# ---------------------------------------------------------------- 5 Rediscovery
n["c5_room"] = node("c5", [
    N("Her room faces the sea. She doesn't turn on the lamp. The moon is enough, laying a silver path across the floor to the foot of the bed."),
    M("I used to dream about this. Not the sex. Just being in a room with you again.", delivery="soft", gesture="chest"),
    V("And the sex?", delivery="playful", expr={"smirk": 0.5}, react=None),
    M("I'm a grown man, Viv. Of course the sex.", react="laugh", head="shake"),
    N("She laughs, and then she isn't laughing, because he has stepped in close and his hand is at the zip at the small of her back."),
    V("Yes. Please.", delivery="whisper", action="step_in", expr={"bite_lip": 0.4}),
    N("The zip slides down. The wine-dark satin loosens, slips from her shoulders, and pools at her feet. Underneath, black lace, chosen this morning by a woman who told herself it didn't mean anything.", action="undress:vivian:outer", shot="two"),
    M("God, look at you.", delivery="breathless", expr={"him.pleasure": 0.5}),
    V("Your turn.", delivery="playful", gesture="reach"),
    N("She undoes his shirt one button at a time, taking her time, the way he used to make her wait. He shrugs it off. His chest is broader than she remembers, softer, warmer.", action="undress:marcus:outer"),
    N("He kisses her neck, her collarbone, the curve of her breast above the lace. When he unhooks it, her breath catches, and she pulls him down with her onto the white sheets.", action="undress:both:under"),
    N("Nothing between them now. His hand slides down between her thighs and finds her already wet, and she arches into his palm with a sound she hasn't made in years.", action="to_bed"),
    V("Don't make me wait any more. I've waited twenty years.", delivery="breathless", expr={"pleasure": 0.5}, vox="moan02"),
], place="hotel_room", location="room four-twelve, the sea outside", mood="candle", ambience="sea", pose="stand",
   act="missionary", actT=0.1, gallery="g_first",
   play={"act": "missionary", "goal": "together", "next": "c6_after",
         "acts": ["missionary", "legsup", "cowgirl", "spoon"],
         "beats": {"start": "c5_b_start", "desire": "c5_b_desire", "edge": "c5_b_edge", "her_peak": "c5_b_peak"}})
n["c5_b_start"] = beat([V("Slowly. I want to feel all of you.", delivery="breathless")])
n["c5_b_desire"] = beat([V("Like that. Just like that, Marcus.", delivery="breathless")])
n["c5_b_edge"] = beat([V("Look at me. Stay with me.", delivery="whisper")])
n["c5_b_peak"] = beat([V("Oh... oh, Marcus.", delivery="breathless"), M("I've got you.", delivery="whisper")])

# ---------------------------------------------------------------- 6 After
n["c6_after"] = node("c6", [
    N("Afterwards they lie tangled in the sheets, the sea breathing in and out beyond the open window. Her head is on his chest. His heart is still running."),
    V("Well. That was better than I remembered.", delivery="soft", expr={"smile": 0.5, "bliss": 0.3}),
    M("We were twenty-two. We didn't know anything.", delivery="soft", react="laugh"),
    V("We knew some things.", delivery="playful", expr={"smirk": 0.4}),
    N("He traces slow circles on her bare shoulder. Neither of them says the thing that is waiting in the room with them. Then, because she always says it first, she does."),
    V("What happens tomorrow?", delivery="tender"),
], place="hotel_room", location="room four-twelve", mood="candle", ambience="sea", pose="lie",
   outfit={"vivian": "nude", "marcus": "nude"},
   choices=[
    C("\"Stay. Take the later ferry. Then we'll see.\"", "c6_stay", trust="+1", honest="+1"),
    C("\"Tomorrow can wait. Kiss me again.\"", "c6_kiss", warmth="+1"),
    C("\"Let's not promise anything. Tonight was enough.\"", "c6_enough"),
   ])
n["c6_stay"] = node("c6", [
    V("Stay. Take the later ferry. And then we'll see. I'm too old to pretend I don't want to see.", delivery="tender"),
    M("I'll take the last one. I'll miss it on purpose if I have to.", delivery="soft", expr={"him.pleasure": 0.2}),
], next="c7_morning")
n["c6_kiss"] = node("c6", [
    V("Tomorrow can wait. Kiss me again.", delivery="whisper", expr={"smile": 0.4}),
    N("He does, slow and thorough, and tomorrow obligingly waits outside the door until the sky starts to grey."),
], next="c7_morning")
n["c6_enough"] = node("c6", [
    V("Let's not promise anything. Tonight was enough. Tonight was perfect.", look="away", delivery="soft"),
    M("Perfect is a lot to live up to.", delivery="soft"),
    V("Then don't try. Just hold me.", delivery="tender"),
], next="c7_morning")

# ---------------------------------------------------------------- 7 Morning
n["c7_morning"] = node("c7", [
    N("Morning comes in pink and gold across the water. Gulls argue on the balcony rail. Somewhere below, the wedding guests are dragging themselves to breakfast."),
    M("Good morning.", delivery="soft", react="smile"),
    V("Good morning. You snore now.", delivery="playful", expr={"smirk": 0.5}),
    M("I've always snored. You used to say it was charming.", react="laugh", gesture="shrug"),
], place="hotel_room", location="room four-twelve, morning", mood="day", ambience="sea", pose="lie", gallery="g_morning",
   choices=[
    C("\"Come to my place next month. Meet my too many books.\"", "c7_second", "trust >= 2 || honest >= 2"),
    C("\"I'll never forget this. Go on, catch your ferry.\"", "c7_onenight"),
   ])
n["c7_second"] = node("c7", [
    V("Come and see me next month. Meet my too many books. Sit on my balcony and watch the evening come in.", delivery="tender", gesture="open"),
    M("I'll come next week. Is next week too soon?", delivery="soft", head="tilt"),
    V("Next week is twenty years late. It's perfect.", expr={"smile": 0.7}, react="laugh"),
    N("He misses the morning ferry. He misses the afternoon one too. When he finally goes, it's with her number written on his palm in eyeliner, just in case."),
], end={"title": "Second Chance", "text": "Some loves aren't lost. They wait, patient as the sea, for two people brave enough to come back to the shore."})
n["c7_onenight"] = node("c7", [
    V("I'll never forget this. Go on. Catch your ferry, before I ask you not to.", delivery="soft", look="away"),
    M("Viv.", delivery="soft"),
    V("Go. Please. Let it be perfect.", delivery="tender"),
    N("He kisses her forehead and goes. She watches the ferry from the balcony until it's a white speck on the blue, and she finds she's smiling."),
], end={"title": "One Perfect Night", "text": "Not every story needs a sequel. Some are whole in a single night, and carried for the rest of a life."})
n["c7_friends"] = node("c7", [
    N("At breakfast he slides into the seat across from her with two coffees, the way he used to, without asking how she takes it. He still remembers."),
    M("I almost knocked last night.", delivery="soft", look="down"),
    V("I almost opened the door.", delivery="soft", expr={"smile": 0.3}),
    M("Then let's start smaller. Write to me. Real letters. Like we're old.", gesture="open", delivery="playful"),
    V("We are old.", react="laugh", expr={"smile": 0.6}),
], place="hotel_terrace", location="the terrace, breakfast", mood="day", ambience="sea", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"},
   end={"title": "Old Friends", "text": "They didn't fall back into bed. They fell back into each other's lives, which is rarer, and might last longer."})

if __name__ == "__main__":
    sys.exit(0 if emit(story, sys.argv[1]) else 1)
