"""The Storm - a novelist alone in a mountain cabin; a knock at the door at midnight in a blizzard."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from storylib import N, V, M, node, beat, C, emit

S = "the_storm"
story = {
    "id": S, "title": "The Storm",
    "tagline": "A novelist alone in a mountain cabin. A knock at the door at midnight.",
    "setting": "A MOUNTAIN CABIN · A WINTER STORM",
    "mood": "candle",
    "cast": {"vivian": {"wardrobe": "knit_sweater_jeans"}, "marcus": {"wardrobe": "flannel_jeans"}},
    "vars": {"trust": 0, "spark": 0, "patient": 0},
    "chapters": [
        {"id": "c1", "title": "The Knock", "start": "c1_knock"},
        {"id": "c2", "title": "Dry Clothes", "start": "c2_fire"},
        {"id": "c3", "title": "Power Out", "start": "c3_dark"},
        {"id": "c4", "title": "Close", "start": "c4_close"},
        {"id": "c5", "title": "Snowed In", "start": "c5_snow"},
        {"id": "c6", "title": "The Second Night", "start": "c6_night"},
        {"id": "c7", "title": "Thaw", "start": "c7_thaw"},
    ],
    "gallery": {"g_fire": "By the fire", "g_night": "The second night", "g_dawn": "Snowbound"},
    "nodes": {},
}
n = story["nodes"]
CABIN = dict(place="cabin_living", location="the cabin, by the fire", mood="candle", ambience="wind+fire")

# ---------------------------------------------------------------- 1 The Knock
n["c1_knock"] = node("c1", [
    N("The storm has been building since dusk. By midnight the cabin sits inside a roaring white nothing, and the only proof of the world is the fire and the scratch of Vivian's pen."),
    N("Chapter nineteen of a novel she has been failing to finish for two years. The heroine is about to make a terrible decision. Vivian is about to make tea."),
    N("Then someone knocks. Three times, hard, the way you knock when you've already knocked softly and nobody heard."),
    M("Hello? I'm sorry, I'm so sorry. My car's in a ditch half a mile down. I saw your light.", delivery="urgent", gesture="open"),
    N("He is tall and frosted white from his hair to his boots, shaking hard enough that his teeth click. He doesn't step forward. He waits."),
], place="cabin_door", location="the cabin door, midnight", mood="candle", ambience="wind+fire",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"}, pose="stand",
   choices=[
    C("\"Get in here before you freeze.\"", "c1_in", trust="+1"),
    C("\"Who are you? What are you doing on this road?\"", "c1_ask", patient="+1"),
   ])
n["c1_in"] = node("c1", [
    V("Get in here, before you freeze to death on my porch and I have to explain it to the sheriff.", gesture="reach", delivery="urgent"),
    M("Thank you. Marcus. My name's Marcus.", head="nod"),
    V("Vivian. Boots off. Stand by the fire. Don't drip on the manuscript.", gesture="point", expr={"smirk": 0.3}),
], next="c2_fire")
n["c1_ask"] = node("c1", [
    V("Who are you? Nobody drives this road in January.", look="partner", expr={"pout": 0.2}),
    M("Marcus Hale. I'm an engineer, I was heading to the dam at Kessler for the morning. The satnav swore this was a short cut.", gesture="explain"),
    M("I'll stand on the porch if you'd rather. I'd just like to call someone.", delivery="soft", look="down"),
    V("There's no signal up here. And there's no porch weather tonight. Come in.", head="shake", gesture="open"),
], next="c2_fire")

# ---------------------------------------------------------------- 2 Dry Clothes
n["c2_fire"] = node("c2", [
    N("Twenty minutes later he's thawing by the hearth in his own flannel shirt, dried stiff and warm over the fire screen, with a mug of tea he holds like a lifeline."),
    M("You write. I didn't mean to look. It's everywhere.", gesture="open", delivery="soft"),
    V("Pages are everywhere. Writing is a generous word for what I've been doing.", gesture="shrug", expr={"smirk": 0.3}),
    M("What's it about?", head="tilt"),
    N("Nobody has asked her that in a long time. Not her agent, who asks when. Not her sister, who asks why. Just what."),
], **CABIN, pose="stand",
   choices=[
    C("Tell him the truth about the book.", "c2_truth", trust="+1", spark="+1"),
    C("\"It's about a man who drives into ditches.\"", "c2_tease", spark="+1"),
   ])
n["c2_truth"] = node("c2", [
    V("It's about a woman who leaves a good marriage for no good reason, and spends a year finding out the reason was her.", delivery="soft", look="away"),
    M("That's not fiction, is it.", delivery="soft"),
    V("Everything is fiction once it's in a book. That's the whole trick.", head="tilt", expr={"smile": 0.3}),
    N("He doesn't say anything clever. He just nods, slowly, like he's put something careful down on a shelf where it won't break."),
], next="c3_dark")
n["c2_tease"] = node("c2", [
    V("It's about a man who drives into ditches in the middle of the night. It's a tragedy.", delivery="playful", expr={"smirk": 0.6}),
    M("Does he at least get a cup of tea at the end?", react="laugh", gesture="shrug"),
    V("He gets a cup of tea in chapter two. The ending isn't written yet.", expr={"smile": 0.5}, look="partner"),
], next="c3_dark")

# ---------------------------------------------------------------- 3 Power Out
n["c3_dark"] = node("c3", [
    N("At one in the morning the lights die with a soft click, and the cabin shrinks to the circle of the fire. The wind finds every gap in the logs and sings through it."),
    V("And there goes the power. It'll be days. It always is.", gesture="shrug"),
    M("I'm sorry. You've got a stranger in your house and no lights.", delivery="soft"),
    V("I've got a stranger who fixes things, I hope. Can you fix a generator?", delivery="playful"),
    M("I can try. In the morning, when I can see my own hands.", react="laugh", gesture="open"),
    N("She lights candles along the mantel. In their light he looks younger, and tired, and kinder than a stranger has any right to look."),
], **CABIN, pose="sit",
   choices=[
    C("Sit beside him on the edge of the bed by the fire.", "c3_close", spark="+1"),
    C("Hand him a blanket and take the armchair.", "c3_apart", patient="+1"),
   ])
n["c3_close"] = node("c3", [
    N("She sits beside him, close enough that their shoulders touch. Neither of them moves away. The fire pops and settles."),
    V("Why were you really driving at midnight in a blizzard, Marcus?", delivery="soft"),
    M("My wife left in the spring. I've been saying yes to every job that takes me far from the house. Tonight it took me a little too far.", delivery="soft", look="down", head="down"),
    V("Then we're two people running from an empty house. In a storm.", delivery="tender", react=None),
    M("Sounds like the start of a novel.", delivery="soft", expr={"him.pleasure": 0.1}),
], next="c4_close", set={"trust": "+1"})
n["c3_apart"] = node("c3", [
    N("She tosses him the wool blanket from the bed and curls into the armchair with her own. They talk across the fire, about nothing, then about everything."),
    M("You're good at this. Listening. Most people wait for their turn.", delivery="soft"),
    V("Writers are thieves. I'm stealing your voice for chapter twenty.", delivery="playful", expr={"smirk": 0.4}),
], next="c4_close")

# ---------------------------------------------------------------- 4 Close
n["c4_close"] = node("c4", [
    N("By three the fire has burned down to a red heart. She should sleep. She doesn't want to. Neither, it seems, does he."),
    M("I should let you sleep. I'll take the floor.", delivery="soft", gesture="open"),
    V("The floor is made of ice. There's a whole bed, and I'm a grown woman.", delivery="soft", expr={"bite_lip": 0.3}),
    N("The candlelight moves on his face. He is looking at her the way people look at something they've decided not to reach for."),
], **CABIN, pose="sit",
   choices=[
    C("Reach for him first.", "c4_kiss", "spark >= 2", spark="+1"),
    C("\"Just sleep, Marcus. Stay on your side.\"", "c4_sleep", patient="+1"),
    C("\"I'm glad you knocked.\" Kiss his cheek.", "c4_cheek", trust="+1"),
   ])
n["c4_kiss"] = node("c4", [
    N("She turns his face to hers with two fingers on his jaw, and kisses him. He tastes of tea and woodsmoke. His hand comes up into her hair, slow, as if she might change her mind.", action="kiss"),
    V("Don't you dare apologise.", delivery="whisper", expr={"smile": 0.4}),
    M("I wasn't going to.", delivery="whisper", action="part"),
    N("They don't go further. Not tonight. They fall asleep half-dressed under the wool blankets, her back against his chest, the storm howling outside like it's jealous."),
], next="c5_snow", gallery="g_fire", set={"kissed": "=1"})
n["c4_sleep"] = node("c4", [
    V("Just sleep. Stay on your side, and if you snore I'm putting you on the porch.", delivery="playful", gesture="point"),
    M("Understood. My side.", react="smile", head="nod"),
    N("They lie back to back under the wool. She listens to him breathe until she doesn't feel like a stranger to him any more."),
], next="c5_snow")
n["c4_cheek"] = node("c4", [
    V("I'm glad you knocked.", delivery="tender"),
    N("She kisses his cheek, rough with a day's stubble. He closes his eyes for a moment longer than a thank-you needs.", action="step_in"),
    M("So am I. More than I can say.", delivery="soft"),
], next="c5_snow")

# ---------------------------------------------------------------- 5 Snowed In
n["c5_snow"] = node("c5", [
    N("Morning. The world outside the windows has been erased and redrawn in white. The car, the road, the dam, all of it is somewhere under four feet of snow."),
    M("I got the generator running. Mostly. The kettle works, the lamp flickers, the radio says two more days.", gesture="explain", delivery="playful"),
    V("Two more days. Trapped with a strange engineer.", expr={"smirk": 0.4}),
    M("I make an excellent omelette. It's my one redeeming feature.", gesture="chest", react="laugh"),
    N("He cooks. She writes. Twice she catches herself reading a paragraph aloud to him, and twice he tells her honestly which line is a lie. Nobody has done that in years."),
    V("You're ruining my process.", delivery="playful", expr={"smile": 0.6}),
    M("Your process was a woman staring at a wall for two years.", delivery="playful", gesture="shrug"),
], place="cabin_living", location="the cabin, morning, snowed in", mood="day", ambience="wind", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"},
   choices=[
    C("\"Tonight, don't take the floor.\"", "c5_ask", "spark >= 1 || trust >= 2", spark="+1"),
    C("\"Stay till the roads open. Then we'll see.\"", "c5_wait", patient="+1"),
   ])
n["c5_ask"] = node("c5", [
    V("Marcus. Tonight, don't take the floor.", delivery="soft", look="partner", expr={"bite_lip": 0.4}),
    M("I wasn't planning to.", delivery="soft", expr={"him.pleasure": 0.2}),
], next="c6_night")
n["c5_wait"] = node("c5", [
    V("Stay till the roads open. Then we'll see what we are when there's somewhere else to be.", delivery="soft"),
    M("That's fair. That's more than fair.", head="nod"),
    N("But the second night comes, and the wind comes with it, and fair turns out not to be what either of them wants."),
], next="c6_night")

# ---------------------------------------------------------------- 6 The Second Night
n["c6_night"] = node("c6", [
    N("The second night the generator gives up for good. Candles again, the fire, the bed piled with every blanket in the cabin."),
    V("Come here. You're too far away.", delivery="whisper", gesture="reach"),
    N("He kneels on the bed in front of her and takes the hem of her sweater in both hands, and waits, the way he waited at the door. She lifts her arms.", action="step_in"),
    N("The sweater comes up and over her head. He undoes the buttons of her jeans with clumsy, careful fingers, laughing softly at himself.", action="undress:vivian:outer"),
    V("Your hands are shaking.", delivery="soft", expr={"smile": 0.4}),
    M("It's cold.", delivery="soft"),
    V("Liar.", delivery="whisper", expr={"smirk": 0.5}),
    N("She pulls the flannel off his shoulders, then the rest. His skin is warm from the fire. When the last of their clothes are on the floor she pushes him down onto his back and climbs over him, her hair falling around both their faces.", action="undress:both:all"),
    N("She guides him into her slowly, watching his face, and the sound he makes is better than any line she has ever written.", action="to_bed"),
    V("Let me. I want to.", delivery="breathless", expr={"pleasure": 0.5, "bite_lip": 0.3}, vox="moan02"),
], place="cabin_bedroom", location="the bedroom, the second night", mood="candle", ambience="wind+fire", pose="sit",
   act="cowgirl", actT=0.2, gallery="g_night",
   play={"act": "cowgirl", "goal": "together", "next": "c7_thaw",
         "acts": ["cowgirl", "missionary", "spoon", "doggy"],
         "beats": {"start": "c6_b_start", "desire": "c6_b_desire", "edge": "c6_b_edge", "her_peak": "c6_b_peak"}})
n["c6_b_start"] = beat([V("Lie back. I've got you.", delivery="breathless")])
n["c6_b_desire"] = beat([V("Don't stop. The storm can have the rest of the world.", delivery="breathless")])
n["c6_b_edge"] = beat([V("Not yet. Wait for me.", delivery="whisper")])
n["c6_b_peak"] = beat([V("Marcus... oh god, Marcus.", delivery="breathless"), M("Vivian.", delivery="whisper")])

# ---------------------------------------------------------------- 7 Thaw
n["c7_thaw"] = node("c7", [
    N("On the fourth day the sun comes out, blinding, and the snowplough grinds up the valley road. Somewhere under a white hump down the hill, his car waits to be dug out."),
    M("The road's open. I heard the plough.", delivery="soft", look="down"),
    V("I heard it too. I was hoping you hadn't.", delivery="soft", expr={"pout": 0.2}),
    N("He's standing by the door in his coat, the way he stood there four nights ago, except this time he isn't asking to come in. He's asking whether to go."),
], place="cabin_living", location="the cabin, the thaw", mood="day", ambience="wind", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"},
   choices=[
    C("\"Stay. Stay till spring. Help me write the ending.\"", "c7_snowbound", "trust >= 2 && spark >= 2"),
    C("\"Go. And come back.\"", "c7_dawn", "spark >= 1"),
    C("\"Write to me. Real letters.\"", "c7_letters"),
   ])
n["c7_snowbound"] = node("c7", [
    V("Stay. Stay till spring. Help me finish the book. I think I know how it ends now.", delivery="tender", gesture="reach"),
    M("How does it end?", delivery="soft", head="tilt"),
    V("With a man who knocks on the right door in the wrong storm.", expr={"smile": 0.7}),
    N("He takes his coat off and hangs it back on the hook by the door, and it stays there until the crocuses come up through the last of the snow."),
], end={"title": "Snowbound", "text": "She finished the novel in April. The dedication reads: for the man in the ditch."}, gallery="g_dawn")
n["c7_dawn"] = node("c7", [
    V("Go. Dig out your car, go and stare at your dam. And then come back.", delivery="soft", gesture="point"),
    M("How do you know I'll come back?", delivery="playful"),
    V("Because you still owe me an omelette.", delivery="playful", expr={"smirk": 0.5}, react="laugh"),
    N("He comes back on Friday, with groceries and a new fuse for the generator and a look on his face like a man walking into his own house."),
], end={"title": "The Morning After", "text": "Some storms take things away. This one, for once, left something on the doorstep."})
n["c7_letters"] = node("c7", [
    V("Write to me. Real letters. I'll write back. Let's find out what we are without a blizzard.", delivery="soft"),
    M("Real letters. With stamps.", head="nod", react="smile"),
    N("The first one arrives a week later. It's six pages long and has a drawing of a car in a ditch in the margin. She reads it four times by the fire."),
], end={"title": "Letters", "text": "They wrote all through the winter. By spring, the letters had stopped being about the storm."})

if __name__ == "__main__":
    sys.exit(0 if emit(story, sys.argv[1]) else 1)
