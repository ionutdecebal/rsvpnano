// Original sample story for testing navigation. No book files or network needed.
const demoChapters = [
  {title:'The open window', paragraphs:[
    'Beyond the garden wall, the morning moved slowly. Light settled on the leaves, and somewhere a small bird began to sing. Mara put her book beside the open window and listened. There was no need to hurry. For a moment, the world was simply this: a quiet room, a warm cup, and the possibility of another page.',
    'The house had belonged to her aunt, who left instructions for almost everything. Feed the birds before breakfast. Turn the brass tap gently. Bring the cushions in before rain. But on the subject of the garden, the instructions were only four words: take your time outside.',
    'Mara had planned to stay for three days. She had brought a small bag, two notebooks, and a list of work she expected to finish. Now the list lay under her cup, its corners curling in the steam. Outside, a path disappeared behind a row of tall rosemary bushes.',
    'At the window she could see the roof of a shed and, farther away, a blue door. Something bright hung from its handle. She thought it might be a ribbon, until the wind turned it and a little square of paper flashed white in the sun.'
  ]},
  {title:'A map in the drawer', paragraphs:[
    'The kitchen drawer resisted, then opened with a soft scrape. Among spare candles and lengths of string lay a folded map. It was drawn in pencil on the back of an envelope. The house stood at one edge; the rest of the page was a tangle of paths, little circles, and names Mara did not recognise.',
    'Someone had marked the orchard with five green dots. Beside a narrow rectangle were the words old glasshouse. A dotted line led beyond it to a place called the listening bench. There was no scale, no arrow pointing north, and no indication of how far any of these things might be.',
    'She took the map outside. The first path was easy to follow, though the rosemary brushed both her sleeves. Bees moved from flower to flower without apparent urgency. At a fork, she stopped to compare the pencil lines with the real paths. Neither branch looked quite like the drawing.',
    'A note in the corner caught her attention: the useful thing about a map is that it gives you somewhere to begin. Mara laughed, folded it carefully, and chose the path that led towards the blue door.'
  ]},
  {title:'The blue door', paragraphs:[
    'The paper on the blue door was a seed packet, empty except for three dark seeds caught in a fold. On the front was a picture of climbing beans. On the back, in her aunt’s handwriting, was a date from the previous spring and a small, confident tick.',
    'The door opened into a walled garden. Beds of dark earth ran in uneven rows. Some were full of green shoots; others held nothing Mara could identify. A pair of gloves lay on a stone, with the fingers pointing towards a watering can. It looked as though someone had just stepped away.',
    'She found the brass key beneath the watering can. Its teeth were broad and worn, and a red thread was tied through the ring. There was no label. Mara tried it in the blue door, but it was much too small. She put it in her pocket beside the map.',
    'At the far end of the garden, a man in a straw hat was repairing a fence. He raised a hand without putting down his hammer. “You must be Mara,” he called. “I wondered when you would find your way out here.”'
  ]},
  {title:'The orchard path', paragraphs:[
    'His name was Ellis. He lived across the lane and had helped with the garden for years. He knew which trees fruited early, where the fox slept, and which paving stone tipped after rain. When Mara showed him the key, he smiled but did not tell her what it opened.',
    '“Try the orchard first,” he said. “You will want to see it before the blossom goes.” He pointed through a gap in the wall, where the path narrowed to a pale strip between the grass. Mara thanked him and set off with the folded map in her hand.',
    'Five apple trees stood in a rough circle. Beneath them, fallen petals lay like scraps of a letter. She walked slowly around each trunk. Every tree wore a little metal tag, but weather had softened the writing until the names were almost impossible to read.',
    'At the last tree she found a wooden box fixed to a post. The lid was fastened with a small lock. The brass key turned easily. Inside was a notebook wrapped in cloth, a pencil, and a photograph of the orchard when the trees were young.'
  ]},
  {title:'Notes on rain', paragraphs:[
    'The first page recorded a week of rain. The second described a late frost. As Mara turned the pages, the entries grew less concerned with weather and more concerned with noticing: the first blackbird at dawn, the shape of a shadow, the smell of the earth after watering.',
    'One entry read: today the rain began before I reached the house, so I stayed in the glasshouse and watched it. I had meant to finish three jobs. Instead I saw a spider repair the same thread four times. I cannot say the afternoon was wasted.',
    'Mara sat beneath the apple tree and read until the light changed. There were blank pages at the back. On one of them, she wrote the date and a single sentence about finding the box. Her handwriting looked hurried beside her aunt’s slow, rounded letters.',
    'Then a drop of water landed on the page. Another touched her wrist. She closed the notebook, wrapped it in its cloth, and looked up. Across the orchard, the glasshouse shone under a gathering sky.'
  ]},
  {title:'Under glass', paragraphs:[
    'The glasshouse door stuck against a tuft of grass. Mara pushed gently until it gave way. Inside, the air smelled of warm wood and damp leaves. Empty pots were stacked beneath a bench. Along the windows, small plants leaned towards the light.',
    'Rain began to tap on the roof. At first she could distinguish each drop. Soon the separate sounds joined into one soft rush. She found a dry place beside the bench and sat down with the notebook on her knees. Through the glass, the orchard became a blur of green and white.',
    'There were labels in a tray: thyme, mint, marigold, climbing bean. She arranged them in a row, then wondered why she had felt the need to arrange anything at all. The garden was full of things growing in the wrong places and doing perfectly well.',
    'When the rain stopped, she waited a little longer. Water fell from the gutter in an uneven rhythm. A bee worked its way along a crack in the door. Somewhere beyond the glass, Ellis began to whistle.'
  ]},
  {title:'The listening bench', paragraphs:[
    'The dotted path on the map began behind the glasshouse. It wound past a compost heap and through a stand of hazel trees. Mara had to push a few branches aside. At the end, on a small rise, stood a bench facing away from the house.',
    'There was nothing remarkable to look at: a field, a hedge, a distant line of roofs. She sat anyway. For a while she heard only the thoughts she had carried with her. Then came the leaves, the birds, a bicycle on the lane, and the low sound of a train beyond the field.',
    'She understood the name now. The listening bench did not offer a view so much as a place to stop seeking one. She put the map away. She did not open the notebook. A cloud passed over the sun and the air grew cool on her hands.',
    'When she stood, she realised she had forgotten to check the time. The discovery made her smile. She followed the path back by memory, pausing once to let a beetle cross a stone.'
  ]},
  {title:'Another page', paragraphs:[
    'By evening, the kitchen smelled of toast and rosemary. Ellis had left a basket of apples by the gate, the last of the previous autumn’s store. Mara cut one in half and ate it standing beside the window. The blue door was only a dark rectangle now.',
    'She looked again at the list beneath her cup. Some of the work still needed doing. Some could wait. She turned the paper over and drew a small map of her own: the window, the drawer, the orchard, the glasshouse, and the bench beyond the trees.',
    'Her map was no more accurate than her aunt’s. The paths bent in the wrong places, and the orchard was much too close to the house. But she knew where each line led. In one corner she wrote: begin here, then see what you find.',
    'Before bed, she set her book beside the open window again. Tomorrow she would read a little, work a little, and go outside. Three days might be enough. It might also be possible to stay for four. She turned off the light and left the question until morning.'
  ]}
];
