"""Neighbours - the newly divorced woman across the hall, a bottle of wine, a thin wall."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from storylib import N, V, M, node, beat, C, emit

S = "neighbours"
story = {
    "id": S, "title": "Neighbours",
    "tagline": "Newly divorced, new flat, the quiet man across the hall. She knocks with a bottle of wine.",
    "setting": "A CITY APARTMENT BUILDING · SPRING",
    "mood": "evening",
    "cast": {"vivian": {"wardrobe": "knit_sweater_jeans"}, "marcus": {"wardrobe": "suit_open_collar"}},
    "vars": {"trust": 0, "spark": 0, "laugh": 0},
    "chapters": [
        {"id": "c1", "title": "The Boxes", "start": "c1_boxes"},
        {"id": "c2", "title": "A Bottle of Wine", "start": "c2_wine"},
        {"id": "c3", "title": "Two Glasses In", "start": "c3_glasses"},
        {"id": "c4", "title": "The Balcony", "start": "c4_balcony"},
        {"id": "c5", "title": "Through the Wall", "start": "c5_wall"},
        {"id": "c6", "title": "Her Place", "start": "c6_her"},
        {"id": "c7", "title": "Saturday Again", "start": "c7_saturday"},
    ],
    "gallery": {"g_balcony": "The balcony", "g_her": "Her place", "g_saturday": "Saturday again"},
    "nodes": {},
}
n = story["nodes"]

# ---------------------------------------------------------------- 1 The Boxes
n["c1_boxes"] = node("c1", [
    N("Fourth floor, no lift. Vivian has been carrying her life up the stairs one box at a time since nine this morning, and her life, it turns out, is mostly books and saucepans."),
    N("Nineteen years of marriage, divided up fairly and kindly by two tired people with a lawyer each. She got the saucepans. He got the dog. She's still not sure who won."),
    N("The box marked KITCHEN, FRAGILE, SORRY splits along the bottom on the landing, just as the door across the hall opens."),
    M("Oh. Oh no. Here, let me...", delivery="urgent", gesture="reach"),
    V("It's fine. It's just plates. Plates are a social construct.", delivery="playful", expr={"smirk": 0.3}, look="down"),
    M("I'm Marcus. Four-B. I'd say welcome to the building, but the building has just eaten your plates.", gesture="open", react="laugh"),
], place="apartment_hall", location="the fourth-floor landing", mood="day", ambience="city",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"}, pose="stand",
   choices=[
    C("Laugh. \"Vivian. Four-A. Help me carry the rest?\"", "c1_laugh", laugh="+1", trust="+1"),
    C("\"Sorry. It's been a long day. A long year.\"", "c1_tired", trust="+1"),
   ])
n["c1_laugh"] = node("c1", [
    V("Vivian. Four-A. I'll forgive the building if you help me carry the rest.", delivery="playful", gesture="point", expr={"smile": 0.5}),
    M("Deal. I'm very strong. I mean, I'm an architect, so I'm not, but I'm enthusiastic.", react="laugh", gesture="shrug"),
    N("He carries eleven boxes up the stairs. He reads the labels out loud as he goes. BATHROOM. BOOKS. MORE BOOKS. DO NOT OPEN, FEELINGS. She hadn't remembered writing that one."),
], next="c2_wine")
n["c1_tired"] = node("c1", [
    V("Sorry. It's been a long day. A long year, honestly.", delivery="soft", look="away"),
    M("Then let me carry the next part of it. Up the stairs, at least.", delivery="soft", head="tilt"),
    N("He carries eleven boxes up four flights and never once asks why a woman her age is moving alone into a one-bedroom flat with two cups and nine saucepans."),
], next="c2_wine")

# ---------------------------------------------------------------- 2 A Bottle of Wine
n["c2_wine"] = node("c2", [
    N("That evening, showered and steadier, she stands outside four-B with a bottle of red she bought for her own housewarming, which nobody is coming to."),
    N("She knocks before she can change her mind."),
    M("Vivian. Hi. Is something else broken?", delivery="soft", react=None),
    V("Only me. I owe you a drink, and I have a bottle and no glasses, because the glasses were in the plate box.", gesture="open", delivery="playful", expr={"smile": 0.4}),
    M("I have glasses. I have two whole glasses.", react="laugh", gesture="chest"),
], place="apartment_hall", location="the door of four-B", mood="evening", ambience="city", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"},
   choices=[
    C("\"Then I'm coming in.\"", "c2_in", spark="+1"),
    C("\"Bring them over. My place looks like a warehouse, it's very romantic.\"", "c2_hers", laugh="+1"),
   ])
n["c2_in"] = node("c2", [
    V("Then I'm coming in.", delivery="playful", action="step_in"),
    N("His flat is the mirror image of hers, except full: drawings pinned to every wall, a piano under the window, a plant that is clearly loved and clearly struggling."),
], next="c3_glasses")
n["c2_hers"] = node("c2", [
    V("Bring them over. My place looks like a warehouse. It's very romantic.", delivery="playful", gesture="point"),
    M("I love a warehouse.", react="laugh"),
    N("They sit on two unopened boxes in her empty living room and drink out of his two whole glasses, and it is, against all odds, a little bit romantic."),
], next="c3_glasses")

# ---------------------------------------------------------------- 3 Two Glasses In
n["c3_glasses"] = node("c3", [
    N("Two glasses in, the light outside has gone blue and the city has switched itself on, window by window."),
    M("Can I ask what happened? You don't have to tell me.", delivery="soft", gesture="open"),
    V("Nothing happened. That's the honest answer. Nineteen years, and one day we looked at each other across the breakfast table and realised we'd been roommates since the kids left.", delivery="soft", look="away"),
    V("So we stopped. Kindly. It was the kindest thing we'd done for each other in years.", delivery="tender"),
    M("That sounds lonelier than a fight.", delivery="soft"),
    V("It was. And you? You don't seem like a man who lives alone by accident.", head="tilt", look="partner"),
    M("I was engaged. She moved to Melbourne for work. I was supposed to follow. I kept finding reasons not to, and eventually she stopped asking.", delivery="soft", look="down", head="down"),
], place="apartment_living", location="four-B, the living room", mood="evening", ambience="city", pose="sit",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"},
   choices=[
    C("\"Then we're both good at leaving kindly.\"", "c3_kind", trust="+1"),
    C("\"Well. Here's to the wrong people, kindly left.\" Raise your glass.", "c3_toast", laugh="+1", spark="+1"),
   ])
n["c3_kind"] = node("c3", [
    V("Then we're both very good at leaving kindly. I'd like to get good at something else.", delivery="soft"),
    M("Like what?", delivery="soft", head="tilt"),
    V("Staying, maybe. When it's worth it.", delivery="tender", look="partner"),
], next="c4_balcony")
n["c3_toast"] = node("c3", [
    V("Well. Here's to the wrong people, kindly left.", delivery="playful", gesture="open", expr={"smirk": 0.4}),
    M("To the wrong people. May they be very happy in Melbourne.", react="laugh"),
    N("They clink his two whole glasses together and laugh until she has to wipe her eyes, and somewhere in the laughing the air in the room changes."),
], next="c4_balcony")

# ---------------------------------------------------------------- 4 The Balcony
n["c4_balcony"] = node("c4", [
    N("The balcony is barely wide enough for two people. They stand there anyway, shoulder to shoulder, the whole city spread below them in gold and red and moving white."),
    V("I've never had a view before. Nineteen years in a house with a garden, and I never once had a view.", delivery="soft"),
    M("You have one now.", delivery="soft"),
    N("He isn't looking at the city when he says it. She notices. She lets him see her notice."),
], place="apartment_balcony", location="the balcony, the city at night", mood="evening", ambience="city", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"},
   choices=[
    C("Kiss him.", "c4_kiss", "spark >= 1 || trust >= 2", spark="+1"),
    C("\"Not tonight. But ask me again.\"", "c4_wait", trust="+1"),
    C("\"Goodnight, neighbour.\"", "c4_night"),
   ])
n["c4_kiss"] = node("c4", [
    N("She kisses him. It's tentative for exactly one second, and then his hand is warm at the side of her neck and the traffic noise falls away completely.", action="kiss"),
    M("I've wanted to do that since the plates.", delivery="whisper", action="part", expr={"him.pleasure": 0.3}),
    V("The plates were a social construct.", delivery="whisper", expr={"smile": 0.6}, react="laugh"),
], gallery="g_balcony", next="c5_wall")
n["c4_wait"] = node("c4", [
    V("Not tonight. But ask me again. Soon.", delivery="soft", expr={"smile": 0.4}),
    M("I'll ask. I'm very persistent. I'm an architect, we're used to planning permission.", delivery="playful", react="laugh"),
], next="c5_wall")
n["c4_night"] = node("c4", [
    V("Goodnight, neighbour. Thank you for the plates.", delivery="soft", gesture="wave"),
    M("I didn't do anything for the plates.", react="smile"),
    V("You did. You laughed.", delivery="tender"),
], next="c5_wall")

# ---------------------------------------------------------------- 5 Through the Wall
n["c5_wall"] = node("c5", [
    N("The walls are thin. All week she hears him through them: the radio in the morning, his phone calls about load-bearing beams, and at night, the piano. Badly. Then, slowly, less badly."),
    N("On Thursday he plays something she recognises, a song from twenty years ago, and she laughs out loud in her empty kitchen, and knocks on the wall twice."),
    N("He knocks back twice. On Friday a note appears under her door. Dinner at yours on Saturday? I'll bring plates."),
    V("He's bringing plates.", delivery="soft", expr={"smile": 0.7}, look="camera"),
], place="apartment_living", location="four-A, a week later", mood="evening", ambience="city", pose="stand",
   show=["vivian"], outfit={"vivian": "dressed"}, next="c6_her")

# ---------------------------------------------------------------- 6 Her Place
n["c6_her"] = node("c6", [
    N("Saturday. The boxes are unpacked. The flat looks like somebody lives here now. He arrives with a bag of groceries and a stack of six white plates tied with string."),
    M("For the collection.", delivery="playful", gesture="open"),
    V("You're ridiculous. Come here.", delivery="soft", expr={"smile": 0.5}, action="embrace"),
    N("Dinner happens, mostly. Halfway through dessert she stands up, takes his hand, and leads him down the short hallway to the bedroom without a word."),
    M("Are you sure?", delivery="soft"),
    V("I've been sure since the plates.", delivery="whisper", action="kiss"),
    N("She pulls the sweater over her head. He unbuttons his shirt, fumbling, laughing at himself, and she steps in to help, and then there's skin against skin and the city glowing on the ceiling.", action="undress:both:outer"),
    N("The rest comes off slowly, between kisses. He lays her back across the bed and kisses down her body, and when his mouth finds her, she forgets nineteen years of anything at all.", action="undress:both:all"),
    V("Marcus. Come here. I want you inside me.", delivery="breathless", expr={"pleasure": 0.6}, action="to_bed", vox="moan02"),
], place="apartment_bedroom", location="four-A, the bedroom", mood="candle", ambience="city", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"}, act="missionary", actT=0.1, gallery="g_her",
   play={"act": "missionary", "goal": "together", "next": "c7_saturday", "acts": ["missionary", "legsup", "doggy", "cowgirl"],
         "beats": {"start": "c6_b_start", "desire": "c6_b_desire", "edge": "c6_b_edge", "her_peak": "c6_b_peak"}})
n["c6_b_start"] = beat([V("The walls are thin. I don't care.", delivery="breathless")])
n["c6_b_desire"] = beat([V("Harder. Let them hear.", delivery="breathless")])
n["c6_b_edge"] = beat([V("Look at me. Stay with me.", delivery="whisper")])
n["c6_b_peak"] = beat([V("Oh god. Oh, Marcus.", delivery="breathless"), M("Vivian.", delivery="whisper")])

# ---------------------------------------------------------------- 7 Saturday Again
n["c7_saturday"] = node("c7", [
    N("Sunday morning. The neighbour downstairs has left a note on the stairwell about noise after midnight. They read it together and laugh until they have to sit down on the steps."),
    M("So. What are we? Neighbours with benefits?", delivery="playful", gesture="shrug"),
    V("I'm too old for a phrase like that.", react=None, expr={"smirk": 0.5}),
], place="apartment_hall", location="the stairwell, Sunday morning", mood="day", ambience="city", pose="stand",
   show=["vivian", "marcus"], outfit={"vivian": "dressed", "marcus": "dressed"}, gallery="g_saturday",
   choices=[
    C("\"Let's leave both doors open. See what happens.\"", "c7_open", "trust >= 2 && spark >= 1"),
    C("\"Breakfast. Every Sunday. At yours or mine.\"", "c7_coffee", "spark >= 1"),
    C("\"Good neighbours. The best kind.\"", "c7_friends"),
   ])
n["c7_open"] = node("c7", [
    V("Let's leave both doors open. Literally. See what happens to the hallway.", delivery="soft", gesture="open"),
    M("The building will talk.", delivery="playful"),
    V("Let it. I've earned some gossip.", expr={"smile": 0.6}, react="laugh"),
    N("By summer the hallway between four-A and four-B has a rug, a plant, and a little table with two whole glasses on it. Nobody remembers whose flat is whose."),
], end={"title": "An Open Door", "text": "She moved in alone with nine saucepans. She found out you can start again at forty-two. It helps to have a neighbour."})
n["c7_coffee"] = node("c7", [
    V("Breakfast. Every Sunday. At yours or mine.", delivery="playful", gesture="point"),
    M("I make terrible coffee.", delivery="playful"),
    V("I know. I can hear you swearing at the machine through the wall.", react="laugh", expr={"smile": 0.6}),
], end={"title": "Morning Coffee", "text": "No grand promises. Just a knock on the wall every Sunday, and two cups, and the city waking up below them."})
n["c7_friends"] = node("c7", [
    V("Good neighbours. The best kind. The kind who bring plates.", delivery="soft"),
    M("I'll always bring plates.", delivery="soft", react="smile"),
    N("They don't become a love story. They become something steadier: two people who check on each other, who knock twice on the wall, who are never quite alone again."),
], end={"title": "Good Neighbours", "text": "Some people come into your life to be your great romance. Some come to carry your boxes up four flights. Both matter."})

if __name__ == "__main__":
    sys.exit(0 if emit(story, sys.argv[1]) else 1)
