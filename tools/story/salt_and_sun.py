"""Salt and Sun - twelve years married, one hard year behind them, three days at a villa by the sea."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from storylib import N, V, M, node, beat, C, emit

S = "salt_and_sun"
story = {
    "id": S, "title": "Salt and Sun",
    "tagline": "Twelve years married, one hard year behind them, three days at a villa by the sea.",
    "setting": "A VILLA BY THE SEA · A LONG WEEKEND",
    "mood": "day",
    "cast": {"vivian": {"wardrobe": "sundress_bikini"}, "marcus": {"wardrobe": "linen_shirt_shorts"}},
    "vars": {"close": 0, "honest": 0, "play": 0},
    "chapters": [
        {"id": "c1", "title": "Arrival", "start": "c1_arrive"},
        {"id": "c2", "title": "Sunscreen", "start": "c2_beach"},
        {"id": "c3", "title": "Siesta", "start": "c3_siesta"},
        {"id": "c4", "title": "The Truth", "start": "c4_terrace"},
        {"id": "c5", "title": "Night Swim", "start": "c5_swim"},
        {"id": "c6", "title": "Our Bed", "start": "c6_bed"},
        {"id": "c7", "title": "Sunday", "start": "c7_sunday"},
    ],
    "gallery": {"g_siesta": "Siesta", "g_bed": "Our bed", "g_sunday": "Sunday"},
    "nodes": {},
}
n = story["nodes"]

# ---------------------------------------------------------------- 1 Arrival
n["c1_arrive"] = node("c1", [
    N("The villa is all white walls and blue shutters, bougainvillea spilling over the gate. Below it the bay is so clear you can count the stones on the bottom."),
    N("It was Marcus's idea. After the year they've had, her mother's illness, his job, the long run of evenings spent talking about schedules and nothing else, he booked it without asking."),
    M("Well? Say something. Say it's terrible and we'll leave.", gesture="open", delivery="playful"),
    V("It's not terrible.", expr={"smile": 0.3}, look="away"),
    M("High praise.", react=None, gesture="shrug"),
    N("He's standing in the doorway with both suitcases, the light behind him. She realises she can't remember the last time she really looked at him."),
], place="villa_terrace", location="a white villa above the bay", mood="day", ambience="sea",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"}, pose="stand",
   choices=[
    C("Take his hand. \"Thank you for this.\"", "c1_hand", close="+1"),
    C("\"Last one in the sea makes dinner.\"", "c1_race", play="+1"),
    C("Unpack first. One thing at a time.", "c1_unpack"),
   ])
n["c1_hand"] = node("c1", [
    V("Thank you. For this. For booking it before I could talk you out of it.", delivery="soft", action="step_in"),
    M("I know you. You would have talked me out of it.", delivery="soft", react="smile"),
    N("His thumb moves across her knuckles, an old habit she'd forgotten she loved."),
], next="c2_beach")
n["c1_race"] = node("c1", [
    V("Last one in the sea makes dinner.", delivery="playful", expr={"smirk": 0.6}, gesture="point"),
    M("That's not fair, I'm holding the suitcases...", delivery="urgent", gesture="shrug"),
    N("She's already gone, sandals in her hand, laughing down the stone steps. He drops the suitcases in the hall and chases her like they're twenty-five."),
], next="c2_beach")
n["c1_unpack"] = node("c1", [
    N("They unpack in a careful, polite silence, two people being considerate of each other the way strangers are."),
    M("Viv. We don't have to do anything this weekend. We could just be here.", delivery="soft", gesture="open"),
    V("I know. I'm trying.", look="down", delivery="soft"),
    M("So am I.", delivery="soft"),
], next="c2_beach")

# ---------------------------------------------------------------- 2 Sunscreen
n["c2_beach"] = node("c2", [
    N("The beach is a crescent of pale sand with nobody on it but them. The sun is fierce; the water is cold enough to make her gasp."),
    N("She steps out of the sundress and stands in the blue bikini, suddenly shy of her own husband, which is absurd after twelve years.", action="undress:vivian:outer"),
    V("Do my back? I'll burn to a crisp otherwise.", delivery="playful", look="away"),
    N("He kneels behind her with the sunscreen. His hands are slow and sure on her shoulders, down the line of her spine, around the edges of the bikini strap."),
    M("You've got freckles here now. New ones.", delivery="soft"),
    V("You've been counting my freckles?", expr={"smile": 0.5}, react=None),
    M("I used to. I'd like to start again.", delivery="tender"),
    N("Something warm unfolds low in her stomach that has nothing to do with the sun."),
], place="villa_beach", location="the little beach below the villa", mood="day", ambience="sea", pose="stand",
   outfit={"vivian": "dressed", "marcus": "dressed"},
   choices=[
    C("Lean back against him and close your eyes.", "c2_lean", close="+1"),
    C("\"Careful, people will talk.\" There's no one for a mile.", "c2_tease", play="+1"),
   ])
n["c2_lean"] = node("c2", [
    N("She leans back into his chest. His arms come around her waist, slick with sunscreen, and they stay like that until the tide reaches their toes.", action="embrace"),
    V("I'd forgotten you were this warm.", delivery="soft", expr={"bliss": 0.3}),
], next="c3_siesta")
n["c2_tease"] = node("c2", [
    V("Careful. People will talk.", delivery="playful", expr={"smirk": 0.6}),
    M("Let them. Let the whole empty beach talk.", react="laugh", gesture="open"),
    N("He kisses the back of her neck, where the sunscreen tastes of coconut, and she laughs so hard she nearly falls into the sea.", action="step_in"),
], next="c3_siesta")

# ---------------------------------------------------------------- 3 Siesta
n["c3_siesta"] = node("c3", [
    N("At three the heat drives them indoors. The shutters turn the light to stripes of gold across the white bed. She lies down just for a minute and he lies down beside her."),
    M("I've missed this. Not just the sex. This. You, next to me, not looking at your phone.", delivery="soft", gesture="chest"),
    V("I've missed you too. I didn't even notice how much until today.", delivery="soft", look="down"),
    N("She rolls toward him and kisses him, lazy and slow, salt still on her lips. He kisses her back like a question.", action="kiss"),
], place="villa_bedroom", location="the shuttered bedroom, afternoon", mood="day", ambience="sea", pose="sit",
   outfit={"vivian": "partial", "marcus": "dressed"},
   choices=[
    C("\"Tonight. I want to wait for tonight.\"", "c3_wait", close="+1"),
    C("Climb into his lap. Tonight is too far away.", "c3_now", "play >= 1 || close >= 2"),
   ])
n["c3_wait"] = node("c3", [
    V("Tonight. I want to wait for tonight. I want to want it all day.", delivery="whisper", action="part", expr={"smile": 0.5}),
    M("You're a cruel woman, Vivian.", delivery="playful", react="laugh"),
], next="c4_terrace")
n["c3_now"] = node("c3", [
    N("The bikini comes off with one tug of a string. His shirt and shorts take a little more effort, and a lot more laughing.", action="undress:both:all"),
    V("Lie back. Let me.", delivery="soft", expr={"smile": 0.6}),
    M("Are you sure? I can...", delivery="soft"),
    V("I'm sure. I want to look at you.", delivery="tender", expr={"pleasure": 0.4}),
    N("She sinks down onto him slowly, her hands flat on his chest, the shutters striping them both with gold.", action="to_bed"),
], act="cowgirl", actT=0.2, gallery="g_siesta",
   play={"act": "cowgirl", "goal": "time:75", "next": "c4_terrace", "acts": ["cowgirl", "revcowgirl", "missionary"],
         "beats": {"start": "c3_b_start", "desire": "c3_b_desire"}})
n["c3_b_start"] = beat([V("Slowly. We have all afternoon.", delivery="breathless")])
n["c3_b_desire"] = beat([V("Mm. There. Right there.", delivery="breathless")])

# ---------------------------------------------------------------- 4 The Truth
n["c4_terrace"] = node("c4", [
    N("Sunset turns the bay to copper. They eat grilled fish with their fingers and drink a cold white wine that tastes of the hillside."),
    M("Can I ask you something? Honestly?", delivery="soft", gesture="open"),
    V("Honestly.", head="nod"),
    M("This year. Were you lonely? With me right there?", delivery="tender", look="partner"),
    N("The question sits on the table between them like a lit candle."),
], place="villa_terrace", location="the terrace at sunset", mood="evening", ambience="sea", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"},
   choices=[
    C("\"Yes. And I was afraid to say it.\"", "c4_yes", honest="+2", close="+1"),
    C("\"We both were. We were surviving, not living.\"", "c4_both", honest="+1"),
    C("\"Let's not do this tonight. Please.\"", "c4_not"),
   ])
n["c4_yes"] = node("c4", [
    V("Yes. I was lonely, and I was afraid to say it, because saying it would make it real.", delivery="soft", look="down", head="down"),
    M("I was too. I kept thinking you'd stopped wanting me, and I didn't know how to ask.", delivery="soft", gesture="chest"),
    V("I never stopped. I just stopped knowing how to show it.", delivery="tender", look="partner"),
    M("Then let's learn again. We're good at learning.", delivery="soft", react="smile", action="step_in"),
], next="c5_swim")
n["c4_both"] = node("c4", [
    V("We both were. We were surviving. Getting through Tuesdays.", gesture="shrug"),
    M("I don't want to get through Tuesdays with you. I want to waste them.", delivery="playful", react="laugh"),
    N("She laughs, and reaches across the table for his hand."),
], next="c5_swim")
n["c4_not"] = node("c4", [
    V("Let's not. Not tonight. Please.", look="away", head="shake"),
    M("All right. Another time.", delivery="soft", head="nod"),
    N("He means it kindly. Still, a small door closes, and they both hear it."),
], next="c5_swim")

# ---------------------------------------------------------------- 5 Night Swim
n["c5_swim"] = node("c5", [
    N("At midnight she takes his hand and leads him down the steps to the water. The moon is nearly full. Nobody for a mile."),
    V("Swim with me.", delivery="whisper", gesture="reach"),
    M("I don't have my...", delivery="soft"),
    V("Neither do I.", delivery="playful", expr={"smirk": 0.5}),
    N("They leave their clothes on the warm stones.", action="undress:both:all"),
    N("The water is silver and black and cold enough to make them both shout, and then laugh at themselves for shouting. Every stroke she makes leaves a trail of green light."),
    M("You're glowing. Look. The plankton.", delivery="soft", gesture="point"),
    N("He swims up behind her and wraps his arms around her waist, and they float like that, two lights in the dark.", action="embrace"),
], place="villa_beach", location="the bay at night", mood="candle", ambience="sea", pose="stand",
   outfit={"vivian": "dressed", "marcus": "dressed"}, next="c6_bed")

# ---------------------------------------------------------------- 6 Our Bed
n["c6_bed"] = node("c6", [
    N("Still damp, still salty, they fall into the big white bed. There is no hurry in it at all."),
    M("I want to remember all of this. Every part of you.", delivery="tender"),
    V("Then take your time. We have all night.", delivery="soft", expr={"smile": 0.6}),
    N("He kisses his way down her body like he is reading something he loves, her throat, her breasts, the soft curve of her belly, and lower, until she is gripping the sheets.", action="to_bed"),
    N("When he finally moves over her and into her, she wraps her legs around him and holds on."),
    V("Don't let go.", delivery="breathless", expr={"pleasure": 0.6}, vox="moan02"),
    M("Never again.", delivery="whisper"),
], place="villa_bedroom", location="the villa bedroom, night", mood="candle", ambience="sea", pose="sit",
   outfit={"vivian": "nude", "marcus": "nude"}, act="missionary", actT=0.1, gallery="g_bed",
   play={"act": "missionary", "goal": "together", "next": "c7_sunday", "acts": ["missionary", "legsup", "spoon", "cowgirl"],
         "beats": {"start": "c6_b_start", "desire": "c6_b_desire", "edge": "c6_b_edge", "her_peak": "c6_b_peak"}})
n["c6_b_start"] = beat([V("Look at me. I want to see you.", delivery="breathless")])
n["c6_b_desire"] = beat([V("Yes. Twelve years and you still know exactly...", delivery="breathless")])
n["c6_b_edge"] = beat([V("Wait. Hold still. Stay right there with me.", delivery="whisper")])
n["c6_b_peak"] = beat([V("Marcus. Oh, Marcus.", delivery="breathless"), M("I love you.", delivery="whisper")])

# ---------------------------------------------------------------- 7 Sunday
n["c7_sunday"] = node("c7", [
    N("Sunday morning, coffee on the terrace, her feet in his lap. The taxi comes at four."),
    M("What do we bring home? Besides the sunburn.", delivery="playful", gesture="open"),
], place="villa_terrace", location="the terrace, Sunday morning", mood="day", ambience="sea", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"},
   choices=[
    C("\"Us. The real us. Let's keep finding each other.\"", "c7_renewed", "honest >= 2"),
    C("\"Promises. Dinner on Fridays. Phones off. A weekend like this every spring.\"", "c7_promises", "close >= 2"),
    C("\"A good memory. Let's see what home brings.\"", "c7_good"),
   ])
n["c7_renewed"] = node("c7", [
    V("Us. The real us. I want to keep finding you, at home, on Tuesdays, everywhere.", delivery="tender", gesture="reach"),
    M("I'm right here. I'm not hard to find. I just needed you to look.", delivery="soft", react="smile"),
    N("They miss the four o'clock taxi on purpose. Then the six o'clock one. They catch the last ferry with ten minutes to spare, laughing, salt in their hair."),
], gallery="g_sunday", end={"title": "Renewed", "text": "Twelve years in, they fell in love with each other again: not the people they used to be, but the ones they are."})
n["c7_promises"] = node("c7", [
    V("Promises. Friday dinners. Phones in a drawer. And this, every spring.", gesture="explain", delivery="playful"),
    M("Every spring. I'll book it before you can talk me out of it.", react="laugh", head="nod"),
    N("She writes it on the villa's notepad and makes him sign it. It goes on the fridge at home, and it stays there for years."),
], end={"title": "Promises", "text": "Love isn't a feeling you keep. It's a promise you keep making, on Fridays, and every spring."})
n["c7_good"] = node("c7", [
    V("A good memory. Let's see what home brings.", delivery="soft", look="away"),
    M("That's fair. It was a good weekend.", head="nod"),
    N("It was. On the ferry back she falls asleep on his shoulder, and he doesn't move for the whole crossing, and neither of them knows yet whether it is a beginning or a pause."),
], end={"title": "A Good Weekend", "text": "Not every question gets answered in three days. But some get asked, and that's where everything starts."})

if __name__ == "__main__":
    sys.exit(0 if emit(story, sys.argv[1]) else 1)
