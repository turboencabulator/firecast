# Message Analysis

I do not have a slave cassette or CD player; all of my information on how one
should work is gathered and reverse-engineered from these data sources:

* <http://stuartschmitt.com/e_and_c_bus/>
* <http://gmlanhack.blogspot.com/2011/01/e-head-unit-logs.html>
* <http://gmlanhack.blogspot.com/2011/02/cemu-aux-v01-files.html>
  * The links to Terry's files are dead, use the
    [Wayback Machine](<https://web.archive.org/web/20170228112214/http://gmlanhack.blogspot.com/2011/02/cemu-aux-v01-files.html>).

Big thanks to Stuart and Terry for their work!

## Frame format

The frame format is described [in detail](<http://stuartschmitt.com/e_and_c_bus/bus_description.html>)
on Stuart's website.  Briefly, each frame contains the following:

* 1 start bit (always 1)
* 2 priority bits
* 6 address bits
* 0-24 data bits (an even number of bits are always sent)
* 1 parity bit (odd parity when the start bit is included in its calculation)

We need to track the 32 bits in the middle.  The start bit is a constant, and
the parity bit can be determined later.  So assume these 32 bits occupy one
32-bit integer.  For uniformity, let's shift it out LSb-first.  Bits 0-1
contain the priority, bits 2-7 contain the address, and bits 8-31 contain the
optional data.  When an even number of these bits has been shifted out and the
remainder is all zero, then we shift out the parity bit and are done with the
frame.

### Format of Stuart's data

The [priority]-[address]-[data...] format of describing messages, as used on
Stuart's website, is somewhat hard to analyze for bit patterns since the
values are given in decimal.  These can be converted to my hexadecimal
representation.  Let's look at his 1-40-63-5 example:

| Part     | Value | LSb-first Binary |
| -------- | ----: | :--------------- |
| Priority |     1 | 10               |
| Address  |    40 | 000101           |
| Data     |    63 | 11111100         |
| Data     |     5 | 10100000         |

The third column is meant to be read in left-to-right, top-to-bottom order.
However the third column is written backwards from how we normally write
integers, with the LSb on the left instead of the right, and zero-padding on
the right instead of the left.  Let's reverse it to the more natural order:

| Part     | Value | Binary   |
| -------- | ----: | -------: |
| Priority |     1 |       01 |
| Address  |    40 |   101000 |
| Data     |    63 | 00111111 |
| Data     |     5 | 00000101 |

Now we can right-shift-out each value in the third column.  Simplifying and
combining the parts, we right-shift-out the bit string
"00000101 00111111 101000 01" which we can represent in hexadecimal as
0x053fa1.  The "a1" in the LSB represents the priority and address; these two
fields will always occupy the LSB.  According to the algorithm, only one of
the five leftmost zeros will be shifted out (to get to an even number of bits
sent).  The other four are just zero-pad.

In my hexadecimal format, BCD data usually reads in natural order.  For
example, the CD changer Load Track command is described as 3-30-49-[XXXXYYYY]
where XXXX is the BCD ones digit and YYYY is the BCD tens digit.  This becomes
0xYX317b where Y and X are the tens and ones digits, respectively.

### Format of Terry's data

The first of Terry's blog pages with E&C data gives an example like this:

	Powering Radio on
	111001101111
	1100001011111110010101
	111001101111

In this format, the data is meant to be read left-to-right in chronological
order.  The values here include the start bit (the first column, note that it
always contains a 1) and the parity bit.  We can check the parity by counting
the number of 1s in each row; it should be odd (and it is for these three
frames).

To convert this to my hexadecimal format, first do the parity check.  Also
count the number of bits; there should be an even number of bits.  If that
succeeds, then we can start to break it apart into different fields:

| Start | Priority | Address | Data         | Parity |
| :---- | :------- | :------ | :----------- | :----- |
| 1     | 11       | 001101  | 11           | 1      |
| 1     | 10       | 000101  | 111111001010 | 1      |
| 1     | 11       | 001101  | 11           | 1      |

Now we can discard the start and parity bits.  For the remainder, reverse the
bits so the right-most bit is shifted out first, and convert it to
hexadecimal.  Note that the second row is identical to Stuart's 1-40-63-5
example.

| Binary                 | Hex     |
| ---------------------: | ------: |
|           11 101100 11 |   0x3b3 |
| 010100111111 101000 01 | 0x53fa1 |
|           11 101100 11 |   0x3b3 |

The second blog page contains a zip with logs and observations.  Here, the bit
strings have been reduced to hexadecimal by assuming the bit string is
right-aligned.  An example from "Tape to FM.txt":

	Play to FM

	E72
	30C075
	E6F
	30C064
	E6F

| Original | Expanded                      |
| -------: | ----------------------------: |
|      E72 |                1110 0111 0010 |
|   30C075 | 0011 0000 1100 0000 0111 0101 |
|      E6F |                1110 0110 1111 |
|   30C064 | 0011 0000 1100 0000 0110 0100 |
|      E6F |                1110 0110 1111 |

Discard the leading zeros (the leftmost 1 is the start bit) and proceed as
before with parity/size checking, dividing into fields, reversing the bits,
and converting to hexadecimal.

| Original | Expanded (start bit first) | Size OK? | Parity OK? | Hex     |
| -------: | :------------------------- | -------- | ---------- | ------: |
|      E72 | 1 11 001110 01           0 | Yes      | Yes        |   0x273 |
|   30C075 | 1 10 000110 000000111010 1 | Yes      | Yes        | 0x5c061 |
|      E6F | 1 11 001101 11           1 | Yes      | Yes        |   0x3b3 |
|   30C064 | 1 10 000110 000000110010 0 | Yes      | Yes        | 0x4c061 |
|      E6F | 1 11 001101 11           1 | Yes      | Yes        |   0x3b3 |

Looking at the frames that end in 0x61, here we can see that a single bit
turned off in the 0x5c061 -> 0x4c061 transition.  This frame sequence is
described as "Play to FM", so we can start making assumptions about the
meaning of that bit.

## Interpreting Terry's data

### First blog post

None of the data in the post contains any size or parity errors.  I have added
whitespace for clarity, to separate out the start/priority/address/data/parity
bits.  I have also added the value in my hexadecimal format, followed by any
existing comments on each line of data after a '#'.

	Radio On:
	1 10 000101 111111001010   1     00053fa1
	Radio Off:
	1 10 000101 111111000010   0     00043fa1

	User keypress or sync or something. Happens after most user key presses:
	1 11 001101 11             1     000003b3

	Cassette Deck Poll?:
	1 11 001110 000010         0     00001073

	CD Transition:
	1 11 001101 11000110       1     000063b3
	1 10 000101 111111001010   1     00053fa1 # (Confirm this?)
	1 11 001101 11000110       1     000063b3

	Cold Start. 12V applied
	1 10 101010                0     00000055
	1 11 001101 11             1     000003b3
	1 11 001101 11             1     000003b3
	1 10 000101 111111         1     00003fa1
	1 11 001110 000010         0     00001073
	1 11 001110 000010         0     00001073
	1 11 001110 000010         0     00001073
	1 11 001110 0010           0     00000473
	1 10 000101 111111000010   0     00043fa1
	1 11 001110 000010         0     00001073
	1 11 001101 11             1     000003b3

	Powering Radio on
	1 11 001101 11             1     000003b3
	1 10 000101 111111001010   1     00053fa1
	1 11 001101 11             1     000003b3

	Powering Radio Off
	1 11 001101 11             1     000003b3
	1 10 000101 111111000010   0     00043fa1

	Most key press events
	1 11 001101 11             1     000003b3

	Aux with no CD (Think this is the look for tape deck command. Will confirm when I get one)
	1 11 001110 000010         0     00001073

	Inserting a CD
	1 11 001101 11000110       1     000063b3
	1 10 000101 111111001010   1     00053fa1
	1 11 001101 11000110       1     000063b3

	Ejecting CD
	1 11 001101 11             1     000003b3

	Remove Ignition
	1 11 001101 11             1     000003b3
	1 10 000101 1111110010     0     00013fa1

	Power on by Ignition
	1 11 001110 000010         0     00001073
	1 11 001101 11             1     000003b3
	1 10 000101 111111001010   1     00053fa1
	1 11 001101 11             1     000003b3

### LogsAndObservations.zip

These files are listed in chronological order by their timestamp.  I've
expanded the given hex values to bits and added whitespace and my hex
representation as above.  Note that some lines have multiple values; for these
I've picked one of the values and used it for expansion, or expanded them on
separate lines.

In several of the first few captures, I noticed size and parity errors.  Many
of these lines look like other existing lines when I shift them to the right
by a few bits.  My assumption is that the first few bits are being missed.
I've marked these with 's', 'p', or 'c' for size errors, parity errors, or
corrections.  Since the missing bits are in the start/priority/address fields
which are more-or-less fixed, I've guessed at what the missing bit values
should be in my hex representation.

Quick guide to error correcting this data:
* If there is a size error, add an odd number of bits, otherwise add an even
  number of bits.
* If there is a parity error, the number of 1 bits added must be odd,
  otherwise the number of 1 bits added must be even.
* We are expanding binary data from hex, and the number of leading zeros is
  ambiguous.  For example a value like '2F' could be interpreted as '101111',
  '0101111', or '00101111'.  The latter two imply that bits added immediately
  to the left must be 0.  The first interpretation does not imply anything.
* Also due to the hex expansion, if the leading digit is 1, 4, 5, 6, or 7,
  when we strip off leading zeros, the number of bits is odd and implies a
  size error.

#### Cassette

	Play Forwards to Reverse Program
	1 11 100101 0000011110     1     0001e0a7 # F283D
	1 11 001110 001110         0     00001c73 # E71C
	1 10 000110 0000101110     1     0001d061 # C305D
	1 11 100101                1     000000a7 # 3CB

	Reverse to Fowards
	1 11 100101 0000011110     1     0001e0a7 # F283D
	1 11 001110 001111         1     00003c73 # E71F
	. 10 000110 00010011100110 1 spc 0019c861 # 4309CD
	1 11 100101                1     000000a7 # 3CB

	Playing status
	Forwards Dolby
	1 10 000110 00010011100010 0     0011c861 # C309C4
	1 10 000110 0001001110     1     0001c861 # C309D
	Reverse Dolby
	1 10 000110 00001011100010 0     0011d061 # C305C4
	1 10 000110 0000101110     1     0001d061 # C305D

	Eject Sequence
	1 11 100101 0000010001     1     000220a7 # F2823
	1 11 100101                1     000000a7 # 3CB
	1 11 001110 0010           0     00000473 # 39C4
	. 10 000110 00100001000001 0 spc 00208461 # 431082
	. .. ...101 11             1   c 000003b3 # 2F
	. .. ...110 0000001010     1   c 00014061 # 3015
	. .. ..1110 000110         1 spc 00001873 # 70D
	. .. ...110 00000010       0   c 00004061 # C04
	. .. ...110 00100001000001 0   c 00208461 # 31082
	. .. ...110 00000010       0   c 00004061 # C04
	1 11 001101 11             1     000003b3 # E6F
	1 10 000110 00100001       1     00008461 # 30C43 - No tape in deck?

	Insert Sequence Dolby
	1 10 000110 00100001000001 0     00208461 # C31082
	1 10 000110 00100001000101 1     00288461 # C3108B
	. .. ...110 00000011       1   c 0000c061 # C07
	. .. ..1110 000111         0 spc 00003873 # 70E
	1 10 000110 00100001000101 1     00288461 # C3108B
	. .. ...110 00000011       1   c 0000c061 # C07
	1 10 000110 001100000001   0     00080c61 # 30C602
	. .. ...110 000000110010   0   c 0004c061 # C064
	1 11 001101 11000001       0     000083b3 # 39B82
	. .. ...110 000000111010   1   c 0005c061 # C075
	. .. ...101 111111001010   1   c 00053fa1 # BF95
	1 11 001110 001111         1     00003c73 # E71F - Play Command
	. 10 000110 0001001110     1 spc 0001c861 # 4309D
	1 11 001101 11000001       0     000083b3 # 39B82
	. .. ...110 0001001110     1   c 0001c861 # 309D

	Insert Sequence No Dolby

	Play from FM Dolby
	1 11 001110 001111         1     00003c73 # E71F - Play Forwards
	. 10 000110 00010011       0 spc 0000c861 # 10C26
	. .. ..1101 11000001       0 spc 000083b3 # 1B82
	. 10 000110 0001001110     1 spc 0001c861 # 4309D (309D)
	1 11 001101 11000001       0     000083b3 # 39B82
	. .. ...110 0001001110     1   c 0001c861 # 309D

	Play Reverse From FM Dolby
	1 11 001110 001110         0     00001c73 # E71C - Play Reverse
	1 10 000110 00001011       0     0000d061 # 30C16
	. .. ..1101 11000001       0 spc 000083b3 # 1B82
	. 10 000110 0000101110     1 spc 0001d061 # 4305D
	1 11 001101 11000001       0     000083b3 # 39B82
	. .. ...110 0000101110     1   c 0001d061 # 305D

	Fast Forwards
	1 11 001110 001010         1     00001473 # E715
	. 10 000110 001101000101   0 spc 000a2c61 # 10C68A
	. .. ...110 0000001110     0   c 0001c061 # 301C

	End Fast Forward
	1 11 001110 001110         0     00001c73 # E71C
	1 10 000110 001100000001   0     00080c61 # 30C602
	. .. ...110 0000101110     1   c 0001d061 # 305D

	Reverse
	1 11 001110 001011         0     00003473 # E716
	1 10 000110 001101001001   0     00092c61 # 30C692
	. .. ...110 0000001110     0   c 0001c061 # 301C

	Cassette to FM Dolby (Same Both Directions)
	1 11 001110 01             0     00000273 # E72 - Stop Command
	1 10 000110 000000111010   1     0005c061 # 30C075
	. .. .....1 11             1  pc 000003b3 # F
	1 10 000110 000000110010   0     0004c061 # 30C064
	1 11 001101 11             1     000003b3 # E6F

#### EandC_Arbids.txt

The analysis is not mine, but the missing bits have made some of the
communication difficult to interpret (seemingly multiple values for a single
command or response).

A couple lines have multiple values, so I've expanded both.

	1 10 101010                0     00000055 # 0x00000354 - Initial Startup - First Command Sent always on powerup
	1 11 001101 11             1     000003b3 # 0x00000E6F - Ack?
	. .1 001101 11             1   c 000003b3 # 0x0000026F - ?
	1 10 000101 111111         1     00003fa1 # 0x0000C2FF - Cd In on powerup
	. .. ...101 111111         1   c 00003fa1 # 0x000002FF - CD In Ignition off
	. 10 000101 111111         1 spc 00003fa1 # 0x000042FF - Radio off Ignition off
	. .. ...101 1111110010     0   c 00043fa1 # 0x00002FE4 - Radio on Ignition off
	1 11 001110 000010         0     00001073 # 0x0000E704 - Cassette Tape Poll - Send when Aux pressed
	. .. ...110 00100001       1   c 00008461 #         0x00000C43 - Cassette Deck Response
	. 10 000110 00100001       1 spc 00008461 #         0x00010C43 - Casette Desk Repsonce to E704

	. 11 001110 0010           0 spc 00000473 # 0x000019C4 - CD In startup sequence
	1 11 001110 0010           0     00000473 # 0x000039C4 - No CD in startup sequence
	1 10 000101 111111000010   0     00043fa1 # 0x0030BF84 - Radio off
	1 10 000101 111111001010   1     00053fa1 # 0x0030BF95 - Radio On
	1 11 001101 11000110       1     000063b3 # 0x00039B8D - CD Playing

	(Rev direction in brackets. Must be encoded in status?)
	1 11 001110 000011         1     00003073 # E707 - Turn off Dolby
	. .. ...110 0001000110     0   c 00018861 # 308C (304CC) - Response Dolby off
	. .. ...110 00001001100110 0   c 00199061
	1 11 001110 000101         1     00002873 # E70B - Turn on Dolby
	. .. ...110 0001001110     1   c 0001c861 # 309D (305CD) - Response Dolby on
	. .. ...110 00001011100110 1   c 0019d061

	Commands to Cassette                      #                         12345678901*--------
	1 11 001110 000010         0     00001073 # E704 - Poll             1110011100000100
	                                          #         (2)
	. 10 000110 00100001       1 spc 00008461 # *10C43  No Cassette
	1 10 000110 00100001       1     00008461 # *30C43  No Cassette

	1 11 001110 000011         1     00003073 # E707 - Dolby Off        1110011100000111
	1 11 001110 000101         1     00002873 # E70B - Dolby On         1110011100001011
	1 11 001110 001111         1     00003c73 # E71F - Play Forwards    1110011100011111
	1 11 001110 001110         0     00001c73 # E71C - Play Reverse     1110011100011100
	1 11 001110 001010         1     00001473 # E715 - Fast Fowards     1110011100010101
	1 11 001110 001011         0     00003473 # E716 - Fast Reverse     1110011100010110
	1 11 001110 01             0     00000273 # E72 - Stop Command      111001110010

	Cassette Arrow Status
	. .. ...110 0000001110     0   c 0001c061 # 301C - Flashing F Arrow 11000000011100
	. .. ...110 0000101110     1   c 0001d061 # 305D - Play Rev         11000001011101
	. .. ...110 0001001110     1   c 0001c861 # 309D - Play Fwd         11000010011101
	. .. ...110 0000001010     1   c 00014061 # 3015 - Eject            11000000010101

#### Cassette_LoadUnload

Here I'm assuming C and R designate who sent the data, the Cassette or Radio.

	Tape in - FM mode Ign off
	1 11 001101 11             1     000003b3 # R - E6F
	1 11 001110 01             0     00000273 # R - E72
	1 10 000101 1111110010     0     00013fa1 # R - C2FE4
	1 10 000110 000000110010   0     0004c061 # C - 30C064

	Tape in - FM mode Ign on
	1 10 000110 001100000001   0     00080c61 # C - 30C602
	1 11 001110 000010         0     00001073 # R - E704
	1 10 000110 001100000001   0     00080c61 # C - 30C602  (Tape?)  110000011000000010
	1 11 001101 11             1     000003b3 # R - E6F
	1 10 000101 111111001010   1     00053fa1 # R - 30BF95
	1 11 001101 11             1     000003b3 # R - E6F

	Ign on after Cassette plugged back in with Tape in
	1 10 000110 00100001000001 0     00208461 # C - C31082
	1 11 001110 000110         1     00001873 # R - E70D
	1 10 000110 00100001000001 0     00208461 # C - C31082
	1 10 000110 00000010       0     00004061 # C?- 30C04
	. 11 001110 000010         0 spc 00001073 # 6704
	1 10 000110 00100001000001 0     00208461 # C - C31082
	1 11 001110 000110         1     00001873 # R - E70D
	1 10 000110 00100001000001 0     00208461 # C - C31082
	1 11 001101 11             1     000003b3 # R - E6F
	1 10 000110 00000010       0     00004061 # ? - 30C04
	1 10 000101 111111001010   1     00053fa1 # R - 30BF95
	1 10 000110 00100001000101 1     00288461 # ? - C3108B
	1 10 000110 00000011       1     0000c061 # ? - 30C07
	1 11 001110 000111         0     00003873 # R - E70E
	1 10 000110 00100001000101 1     00288461 # C?- C3108B
	1 10 000110 00000011       1     0000c061 # ? - 30C07
	1 11 001101 11             1     000003b3 # R - E6F
	1 10 000110 001100000001   0     00080c61 # C - 30C602
	1 10 000110 000000110010   0     0004c061 # C - 30C064
	1 10 000101 111111001010   1     00053fa1 # C - 30BF95

	Ign off no tape
	1 11 001101 11             1     000003b3 # R - E6F
	1 10 000101 1111110010     0     00013fa1 # R - C2FE4

	Ign on no tape
	1 11 001110 000010         0     00001073 # R - E704
	1 10 000110 00100001       1     00008461 # C - 30C43 (No tape)     110000110001000011
	1 11 001101 11             1     000003b3 # R - E6F
	1 10 000101 111111001010   1     00053fa1 # R - 30BF95
	1 11 001101 11             1     000003b3 # R - E6F

#### Cold Power Up to Play

By this point it looks like the problem of missing bits has been fixed.

	1 10 101010                0     00000055 # 354
	1 11 001101 11             1     000003b3 # E6F
	1 11 001101 11             1     000003b3 # E6F
	1 10 000101 111111         1     00003fa1 # C2FF
	1 10 000110 00100001000001 0     00208461 # C31082
	1 11 001110 000010         0     00001073 # E704
	1 10 000110 00100001000001 0     00208461 # C31082
	1 10 000110 00100001000101 1     00288461 # C3108B
	1 10 000110 00000011       1     0000c061 # 30C07
	1 11 001110 000111         0     00003873 # E70E
	1 10 000110 00100001000101 1     00288461 # C3108B
	1 10 000110 00000011       1     0000c061 # 30C07
	1 10 000110 001100000001   0     00080c61 # 30C602
	1 10 000110 000000110010   0     0004c061 # 30C064
	1 10 000101 111111000010   0     00043fa1 # 30BF84
	1 11 001110 000010         0     00001073 # E704
	1 10 000110 001100000001   0     00080c61 # 30C602
	1 11 001101 11000001       0     000083b3 # 39B82
	1 10 000110 000000111010   1     0005c061 # 30C075
	1 11 001110 001111         1     00003c73 # E71F
	1 10 000110 0001001110     1     0001c861 # C309D
	1 11 001101 11000001       0     000083b3 # 39B82
	1 10 000110 0001001110     1     0001c861 # C309D
	1 10 000101 111111001010   1     00053fa1 # 30BF95
	1 10 000110 00010011100010 0     0011c861 # C309C4
	1 10 000110 00010011100110 1     0019c861 # C309CD
	1 10 000110 0001001110     1     0001c861 # C309D

#### Powerup.txt

	1 10 101010                0     00000055 # 354
	1 11 001101 11             1     000003b3 # E6F
	1 11 001101 11             1     000003b3 # E6F
	1 10 000101 111111         1     00003fa1 # C2FF
	1 10 000110 00100001000001 0     00208461 # C31082
	1 11 001110 000010         0     00001073 # E704
	1 10 000110 00100001000001 0     00208461 # C31082
	1 10 000101 111111000010   0     00043fa1 # 30BF84
	1 11 001110 000010         0     00001073 # E704
	1 11 001101 11             1     000003b3 # E6F
	1 10 000110 00100001000001 0     00208461 # C31082
	1 10 000110 00100001000101 1     00288461 # C3108B
	1 10 000110 00000011       1     0000c061 # 30C07
	1 11 001110 000111         0     00003873 # E70E
	1 10 000110 00100001000101 1     00288461 # C3108B
	1 10 000110 00000011       1     0000c061 # 30C07
	1 10 000110 001100000001   0     00080c61 # 30C602
	1 10 000110 000000110010   0     0004c061 # 30C064
	1 11 001101 11000001       0     000083b3 # 39B82
	1 10 000110 000000111010   1     0005c061 # 30C075
	1 11 001110 001111         1     00003c73 # E71F
	1 10 000110 0001001110     1     0001c861 # C309D
	1 11 001101 11000001       0     000083b3 # 39B82
	1 10 000110 0001001110     1     0001c861 # C309D

#### Tape to FM.txt

	Play to FM
	1 11 001110 01             0     00000273 # E72
	1 10 000110 000000111010   1     0005c061 # 30C075
	1 11 001101 11             1     000003b3 # E6F
	1 10 000110 000000110010   0     0004c061 # 30C064
	1 11 001101 11             1     000003b3 # E6F

	FM to play
	1 11 001110 001111         1     00003c73 # E71F
	1 10 000110 00010011       0     0000c861 # 30C26
	1 11 001101 11000001       0     000083b3 # 39B82
	1 10 000110 0001001110     1     0001c861 # C309D
	1 11 001101 11000001       0     000083b3 # 39B82
	1 10 000110 0001001110     1     0001c861 # C309D

#### CassetteCommands

In this file the hex values are already expanded to bits (and in at least one
case the hex value in the comment doesn't match the bits).  I've added
whitespace and my hex value.  The bit meaning analysis is not mine.

	Command Feedback Packet (DD Direction) d=dolby
	              ADDSdC       P
	1 10 000110 00010011       0     0000c861 # (FD) 30C26 (Result from E71F)
	1 10 000110 00001011       0     0000d061 # (RD) 30C16 (Result from E71C)
	1 10 000110 00010001       1     00008861 # (F)  30C23 (Result from E71F)
	1 10 000110 00001001       1     00009061 # (R)  30C13 (Result from E71C)
	1 10 000110 00000010       0     00004061 # Ejecting 30C04
	1 10 000110 00100001       1     00008461 # Ejected  30C43 (Empty Deck)
	1 10 000110 00000011       1     0000c061 # Loading of tape 30C07 Loading
	1 10 000110 00000001       0     00008061 # Loading of Tape 30C02 Non Dolby

	              ADDSdCUV     P
	1 10 000110 0001001110     1     0001c861 # (FDolby) C309D (Used for playing indicator)
	1 10 000110 0000101110     1     0001d061 # (RDdolby) C305D (Used as playing indicator)
	1 10 000110 0000001110     0     0001c061 # (FF or Rew Seeking) C301C Seeking.
	1 10 000110 0001000110     0     00018861 # (F) C308C
	1 10 000110 0000100110     0     00019061 # (R) C304C
	1 10 000110 0000001111     1     0003c061 # End of Seek F/R C301F (Stopped?)
	1 10 000110 0000001010     1     00014061 # Reverse Play Eject
	1 10 000110 0000000111     0     00038061 # Seen going from Fwd to Rev auto C300E Non Dolby (Stopped C301F)
	1 10 000110 0000000010     0     00010061 # C3004 Reverse Dolby Eject

	              ADDSdCUVWX
	1 10 000110 001101000101   0     000a2c61 # (Command result from E715) 30C68A
	1 10 000110 001101001001   0     00092c61 # (Command result from E716) 30C692
	1 10 000110 001100000001   0     00080c61 # (Command Result from FF returning to play) 30C602 (Powers up with cassette in)
	1 10 000110 000000111010   1     0005c061 # (Move to play) 30C075
	1 10 000110 000000011010   0     00058061 # (Ready to play Non Dolby) 30C043
	1 10 000110 000000110010   0     0004c061 # (Rest State) 30C064
	1 10 000110 000000010010   1     00048061 # (Non Dolby rest state) 30C064
	1 10 000110 001110000001   1     00081c61 # (Seeking Fowards) 30C703
	1 10 000110 001100100001   1     00084c61 # (End of tape flip ) 30C643

	              ADDSdCUVWXYZ
	1 10 000110 00001011100110 1     0019d061 # (Reverse Dolby Silence?) C305CD
	1 10 000110 00001011100010 0     0011d061 # (Reverse Dolby Silence?) C305C4
	1 10 000110 00010011100010 0     0011c861 # (Forward Dolby Silence) C309C4
	1 10 000110 00010011100110 1     0019c861 # (Forward DOlby Silence) C309CD
	1 10 000110 00010001100010 1     00118861 # (Forward No Dolby Silence) C308C5
	1 10 000110 00100001000001 0     00208461 # (Ejecting/Loading) C31082
	1 10 000110 00100001000101 1     00288461 # (Next in sequence C31082->C3108B) C3108B

	A = Mute Audio?
	DD = Direction*
	        10 Forwards
	        01 Reverse
	        00 None
	        11 Seeking Forwards

	S = Seek*
	d = Dolby*
	C = Cassette Status (1 in)
	V
	W
	X
	Y
	Z = Loading

	Ack? (39B82)               P
	1 11 001101 11000001       0     000083b3

	1 11 100101 0000011110     1     0001e0a7 # (Program Pressed on deck) F283D
	1 11 100101 0000010001     1     000220a7 # (Eject pressed on deck) F2823
	1 11 100101                1     000000a7 # (Post Program set)

	Radio Commands
	                 p         P
	1 11 001110 01             0     00000273 # Stop Playing   E72
	1 11 001110 000110         1     00001873 # Eject E70D
	1 11 001110 000111         0     00003873 # Loading Tape? E70E - Radio Ack that there is a tape in there.
	1 11 001110 001111         1     00003c73 # (Play F) E71F
	1 11 001110 001110         0     00001c73 # (Play R) E71C
	1 11 001110 000101         1     00002873 # (Enable Dolby) E70B
	1 11 001110 000011         1     00003073 # (Disable Dolby) E707
	1 11 001110 000010         0     00001073 # (Poll for Cassette)  E704
	1 11 001110 001010         1     00001473 # (Seek Rewind)      E715
	1 11 001110 001011         0     00003473 # (Seek Forward)     E716
	1 11 001110 0010           0     00000473 # (Eject?)           39C4

	Parity bit (Even = 1 / Odd = 0)

#### Load_UnloadSequences.txt

A couple of the lines have multiple hex values that differ only by the Dolby
bit.  I've expanded both.

	Load:

	Cassette Feedback load
	1 10 000110 00000011       1     0000c061 # 30C07 (30C02) Non Dolby
	1 10 000110 00000001       0     00008061
	1 10 000110 00000011       1     0000c061 # 30C07

	Motor Statuses
	1 10 000110 00100001000001 0     00208461 # C31082
	1 10 000110 00100001000101 1     00288461 # C3108B
	1 10 000110 00100001000101 1     00288461 # C3108B
	1 10 000110 001100000001   0     00080c61 # 30C602
	1 10 000110 000000110010   0     0004c061 # 30C064/30C025 (have Dolby)
	1 10 000110 000000010010   1     00048061
	1 10 000110 000000111010   1     0005c061 # 30C075/30C034 (Have Dolby)
	1 10 000110 000000011010   0     00058061

	Unload:
	Cassette Feedback
	1 10 000110 00000010       0     00004061 # 30C04
	1 10 000110 00000010       0     00004061 # 30C04
	1 10 000110 00000010       0     00004061 # 30C04
	1 10 000110 00100001       1     00008461 # 30C43

	Motor Status
	1 10 000110 00100001000001 0     00208461 # C31082

### Cemu code

There are a couple flavors of the Cemu code.  Both have the following sections
and are nearly identical.  Below is the cemu_attiny code.  Note that the
ArduinoCode has a newer timestamp and a few of these lines missing, in
particular the delay in initCassette and the call to initCassette in the first
arm of the switch statement.

	                                          # void initCassette()
	                                          # {
	1 10 000110 00100001000001 0     00208461 #     sendE_C(0x00C31082, 23); //Gets E70E everytime!
	1 11 001110 000111         0     00003873 #     _delay_us(1000);
	1 10 000110 00100001000101 1     00288461 #     sendE_C(0x00C3108B, 23);
	                                          # }

	                                          # void processResult(uint32_t packet)
	                                          # {
	                                          #   switch (packet) {
	1 11 001110 000010         0     00001073 #   case 0x0000E704:
	                                          #     //Serial.println("IC");
	                                          #    //delay(50);
	1 10 000110 000000111010   1     0005c061 #    //sendE_C(0x0030C075, 22);
	                                          #     initCassette();
	                                          #     break;

	                                          #   // Case Cold Start
	1 11 001110 000111         0     00003873 #   case 0x0000E70E: // Theres a tape dear liza..
	1 10 000110 001100000001   0     00080c61 #     sendE_C(0x0030C602, 21);
	1 10 000110 000000110010   0     0004c061 #     sendE_C(0x0030C064, 21);
	1 10 000110 000000111010   1     0005c061 #     sendE_C(0x0030C075, 21);
	                                          #     break;

	1 11 001110 000110         1     00001873 #   case 0x0000E70D: // Theres a tape dear liza..
	1 10 000110 001100000001   0     00080c61 #     sendE_C(0x0030C602, 21);
	1 10 000110 000000110010   0     0004c061 #     sendE_C(0x0030C064, 21);
	1 10 000110 000000111010   1     0005c061 #     sendE_C(0x0030C075, 21);
	                                          #     break;

	1 11 001110 001011         0     00003473 #   case 0x0000E716: // Fast Forwards
	1 10 000110 001101001001   0     00092c61 #     sendE_C(0x0030C692, 21);
	1 10 000110 0000001110     0     0001c061 #     sendE_C(0x000C301C, 19);
	1 10 000110 001110000001   1     00081c61 #     sendE_C(0x0030C703, 21);
	1 10 000110 0000001111     1     0003c061 #     sendE_C(0x000C301F, 19);
	                                          #     // Call Reverse function here.
	                                          #     break;

	1 11 001110 001010         1     00001473 #   case 0x0000E715: // Reverse
	1 10 000110 001101000101   0     000a2c61 #     sendE_C(0x0030C68A, 21);
	1 10 000110 0000001110     0     0001c061 #     sendE_C(0x000C301C, 19);
	1 10 000110 001110000001   1     00081c61 #     sendE_C(0x0030C703, 21);
	1 10 000110 0000001111     1     0003c061 #     sendE_C(0x000C301F, 19);
	                                          #     break;

	1 11 001110 001101         0     00002c73 #   case 0x0000E71A: // FFWD
	1 10 000110 001100001001   1     00090c61 #     sendE_C(0x0030C613, 21);
	1 10 000110 0000001110     0     0001c061 #     sendE_C(0x000C301C, 19);
	1 10 000110 001100100001   1     00084c61 #     sendE_C(0x0030C643, 21);
	                                          #     break;

	1 11 001110 0011           1     00000c73 #   case 0x000039C7: // RRWD
	1 10 000110 001100000101   1     000a0c61 #     sendE_C(0x0030C60B, 21);
	1 10 000110 0000001110     0     0001c061 #     sendE_C(0x000C301C, 19);
	1 10 000110 001100100001   1     00084c61 #     sendE_C(0x0030C643, 21);
	                                          #     break;

	1 11 001110 001110         0     00001c73 #  case 0x0000E71C: // Send the play and the flip..
	1 10 000110 0000101110     1     0001d061 #     sendE_C(0x000C305D, 19);
	1 10 000110 001100100001   1     00084c61 #     sendE_C(0x0030C643, 21);
	                                          #     break;

	1 11 001110 001111         1     00003c73 #   case 0x0000E71F: // Theres a tape dear liza..
	1 10 000110 00010011       0     0000c861 #     sendE_C(0x00030C26, 17);
	1 10 000110 0001001110     1     0001c861 #     //sendE_C(0x000C309D, 19);
	                                          #     break;

	1 11 001101 11000001       0     000083b3 #   case 0x00039B82:
	1 10 000110 0001001110     1     0001c861 #     sendE_C(0x000C309D, 19);
	                                          #     break;

	1 11 001110 01             0     00000273 #   case 0x00000E72: // Stop playing..
	1 10 000110 000000111010   1     0005c061 #     sendE_C(0x0030C075, 21);
	1 10 000110 000000110010   0     0004c061 #     sendE_C(0x0030C064, 21);
	                                          #     break;

	                                          #   default:
	                                          #     // if nothing else matches, do the default
	                                          #     // default is optional
	                                          #     break;
	                                          #   }
	                                          # }

## Stuart's data

	Information from OnStar
	1 10 110010 0110                     0 0000064d # 1-19-6     No session active
	1 10 110010 0110000001               1 0002064d # 1-19-6-2   Status; radio mutes audio and enables controls
	1 10 110010 01100000000010           1 0010064d # 1-19-6-16  Status; radio mutes audio and disables controls
	1 10 110010 01100000010010           0 0012064d # 1-19-6-18  Status; radio unmutes audio and disables radio controls
	1 10 110010 001010                   0 0000144d # 1-19-20    Status; radio disables SOURCE button during OnStar session
	1 10 110010 001010000001             1 0008144d # 1-19-20-8  Status; radio enables SOURCE button during OnStar session (see 3-51-127) 

	Information from cassette player
	1 10 000110                          1 00000061 # 1-24-...  Future work: decoding from Terry Kolody's observations.

	Information from CD changer
	1 10 010110 1000000ABCDEFGHI         1 00000169 # 1-26-1/7-[ABCDEFGHI]  Disc read status:
	                   1                       8    #   A  laser on
	                    1                     1     #   B  playing
	                     1                    2     #   C  ready to seek
	                      1                   4     #   D  done seeking
	                       1                  8     #   E  always 0?
	                        1                1      #   F  always 0?
	                         1               2      #   G  always 0?
	                          1              4      #   H  disc data does not need updating
	                           1             8      #   I  battery was disconnected
	1 10 010110 1001000ABCDEFGHIJK       0 00000969 # 1-26-9/7-[ABCDEFGHIJK]  Mechanism status:
	                   1                       8    #   A  ready
	                    1                     1     #   B  forward seeking
	                     1                    2     #   C  reverse seeking
	                      1                   4     #   D  disc changer busy
	                       1                  8     #   E  magazine ejector busy
	                        1                1      #   F  disc data does not need updating
	                         1               2      #   G  always 0?
	                          1              4      #   H  magazine is present
	                           1             8      #   I  playback not possible
	                            1           1       #   J  always 0?
	                             1          2       #   K  door is open
	1 10 010110 1011100000ABCDEFGHIJKL   0 00001d69 # 1-26-29/7-0/3-[ABCDEFGHIJKL]  Discs present:
	                      1                   4     #   A  disc 1
	                       1                  8     #   B  disc 2
	                        1                1      #   C  disc 3
	                         1               2      #   D  disc 4
	                          1              4      #   E  disc 5
	                           1             8      #   F  disc 6
	                            1           1       #   G  disc 7
	                             1          2       #   H  disc 8
	                              1         4       #   I  disc 9
	                               1        8       #   J  disc 10
	                                1      1        #   K  disc 11
	                                 1     2        #   L  disc 12
	1 10 010110 1011100100XXXXXXXXXX     1 00009d69 # 1-26-29/7-1/3-[XXXXXXXXXX]  Random mode (response to 3-30-93-...)
	                      ..........        XXX     #   XXXXXXXXXX  RNG seed (0 if random mode off)
	1 10 010110 1011100110XXXXXXXXXX     0 00019d69 # 1-26-29/7-3/3-[XXXXXXXXXX]  Random mode (response to 3-30-221-...)
	                      ..........        XXX     #   XXXXXXXXXX  RNG seed (0 if random mode off)

	Information from radio
	1 10 000101 11111100XYZ0             1 00003fa1 # 1-40-63-[XYZ]  Power status
	                    1                     1     #   X  audio system on
	                     1                    2     #   Y  OnStar session active
	                      1                   4     #   Z  vehicle accessory power on

	Information from radio
	1 10 100101 01                       1 000002a5 # 1-41-2  OnStar audio mode active
	1 10 100101 0110                     0 000006a5 # 1-41-6  OnStar audio mode inactive

	Information from radio in response to OnStar
	1 10 000011 0110                     1 000006c1 # 1-48-6     OnStar audio enabled
	1 10 000011 0111010010               0 00012ec1 # 1-48-46-1  Acknowledge OnStar call in progress

	Information from radio for IPM
	1 10 100111 01100110                 1 000066e5 # 1-57-102        No personalization loaded
	1 10 100111 0110010XX0               0 000026e5 # 1-57-38/7-[XX]  Loaded personalization
	                   ..                     XX    #   XX  memory number 1 or 2

	Information from CD changer
	1 01 010110 1001100XXXXYYY           1 0000196a # 2-26-25/7-[XXXXYYY]    Total disc time, frames part
	                   ....                   XX    #   XXXX  BCD ones digit
	                       ...               YY     #   YYY   BCD tens digit (0-7)
	1 01 010110 1001010XXXXYYYY0         1 0000296a # 2-26-41/7-[XXXXYYYY]   Disc number
	                   ....                   XX    #   XXXX  BCD ones digit
	                       ....              YY     #   YYYY  BCD tens digit
	1 01 010110 1000110XXXXYYYY0         1 0000316a # 2-26-49/7-[XXXXYYYY]   Playback track
	                   ....                   XX    #   XXXX  BCD ones digit
	                       ....              YY     #   YYYY  BCD tens digit
	1 01 010110 101111                   1 00003d6a # 2-26-61                During playback, start of a new minute
	1 01 010110 10000010XXXXYYYY         0 0000416a # 2-26-65-[XXXXYYYY]     Playback time, minutes part
	                    ....                  X     #   XXXX  BCD ones digit
	                        ....             Y      #   YYYY  BCD tens digit
	1 01 010110 10010010XXXXYYYY         1 0000496a # 2-26-73-[XXXXYYYY]     Playback time, seconds part
	                    ....                  X     #   XXXX  BCD ones digit
	                        ....             Y      #   YYY   BCD tens digit (0-5)
	1 01 010110 10001011                 0 0000d16a # 2-26-209               Begin disc data (note that this is 2-26-81/7-1)
	1 01 010110 1001101XXXXYYYY0         0 0000596a # 2-26-89/7-[XXXXYYYY]   Disc track count
	                   ....                   XX    #   XXXX  BCD ones digit
	                       ....              YY     #   YYYY  BCD tens digit
	1 01 010110 1000011XXXXYYYY0         1 0000616a # 2-26-97/7-[XXXXYYYY]   Total disc time, minutes part
	                   ....                   XX    #   XXXX  BCD ones digit
	                       ....              YY     #   YYYY  BCD tens digit
	1 01 010110 1001011XXXXYYYY0         0 0000696a # 2-26-105/7-[XXXXYYYY]  Total disc time, seconds part
	                   ....                   XX    #   XXXX  BCD ones digit
	                       ....              YY     #   YYYY  BCD tens digit

	Information from radio
	1 01 000101 10XXXXXXYYYYYZZZZZZ0     0 000001a2 # 2-40-1/2-[XXXXXXYYYYYZZZZZZ]  Clock time
	              ......                       XX   #   XXXXXX  minutes (0-59)
	                    .....                YY     #   YYYYY   hours (0-23)
	                         ......         ZZ      #   ZZZZZZ  days (0-63)

	Commands to cassette
	1 11 001110                          1 00000073 # 3-28-...  Future work: decoding from Terry Kolody's observations.

	Commands to CD changer
	1 11 011110 10                       1 0000017b # 3-30-1                   Forward or reverse scan button released
	1 11 011110 1001                     0 0000097b # 3-30-9                   Request status, track, and minute
	1 11 011110 100010                   0 0000117b # 3-30-17                  Stop (no magazine present)
	1 11 011110 1000010010               1 0001217b # 3-30-33-1                Fast forward scan
	1 11 011110 1001010010               0 0001297b # 3-30-41-1                Fast reverse scan
	1 11 011110 10001100XXXXYYYY         1 0000317b # 3-30-49-[XXXXYYYY]       Load track
	                    ....                  X     #   XXXX  BCD ones digit
	                        ....             Y      #   YYYY  BCD tens digit
	1 11 011110 101111                   1 00003d7b # 3-30-61                  Data received OK
	1 11 011110 100000100XXXXYYYY0       0 0000417b # 3-30-65-0/1-[XXXXYYYY]   Seek to time, minutes part
	                     ....                XX     #   XXXX  BCD ones digit
	                         ....           YY      #   YYYY  BCD tens digit
	1 11 011110 10111010XXXXXXXXXX       1 00005d7b # 3-30-93-[XXXXXXXXXX]     Random mode
	                    ..........          XXX     #   XXXXXXXXXX  RNG seed (0 if random mode off)
	1 11 011110 10001001                 1 0000917b # 3-30-145                 Stop playback
	1 11 011110 10011001                 0 0000997b # 3-30-153                 Is module present?
	1 11 011110 10011001111111           0 003f997b # 3-30-153-63              Upload disc data
	1 11 011110 1000010110               0 0001a17b # 3-30-161-1               2× fast forward scan
	1 11 011110 1001010110               1 0001a97b # 3-30-169-1               2× fast reverse scan
	1 11 011110 10001101XXXXYYYY         0 0000b17b # 3-30-177-[XXXXYYYY]      Load track and seek to 0:00
	                    ....                  X     #   XXXX  BCD ones digit
	                        ....             Y      #   YYYY  BCD tens digit
	1 11 011110 1001001101XXXXYYYY       1 0002c97b # 3-30-201-2/2-[XXXXYYYY]  Seek to time, seconds part
	                      ....               XX     #   XXXX  BCD ones digit
	                          ....          YY      #   YYYY  BCD tens digit
	1 11 011110 100110110XXXXYYYY0       1 0000d97b # 3-30-217-0/1-[XXXXYYYY]  Load disc
	                     ....                XX     #   XXXX  BCD ones digit
	                         ....           YY      #   YYYY  BCD tens digit
	1 11 011110 10111011XXXXXXXXXX       0 0000dd7b # 3-30-221-[XXXXXXXXXX]    Random mode
	                    ..........          XXX     #   XXXXXXXXXX  RNG seed (0 if random mode off)

	Command to radio from OnStar
	1 11 000101 11111110                 1 00007fa3 # 3-40-127  Acknowledge power status

	Commands from radio
	1 11 100101 11                       1 000003a7 # 3-41-3     Temperature button released
	1 11 100101 1100000001               0 000203a7 # 3-41-3-2   Temperature down button pressed
	1 11 100101 1001000001               0 000209a7 # 3-41-9-2   Eject CD changer magazine
	1 11 100101 1100100001               1 000213a7 # 3-41-19-2  Temperature up button pressed

	Commands from or to radio
	1 11 001101 11                       1 000003b3 # 3-44-3    From radio: No auxiliary audio selected
	1 11 001101 11000110                 1 000063b3 # 3-44-99   From radio: CD changer selected
	1 11 001101 00011110                 1 000078b3 # 3-44-120  OnStar to radio: Disable recurring power status messages (keepalive pings)
	1 11 001101 00111110                 0 00007cb3 # 3-44-124  OnStar to radio: Enable recurring power status messages (keepalive pings)
	1 11 001101 11111110                 0 00007fb3 # 3-44-127  To radio: Send one power status message
	1 11 001101 11000101                 1 0000a3b3 # 3-44-163  From radio: OnStar selected
	1 11 001101 10000011                 0 0000c1b3 # 3-44-193  From radio: Cassette selected

	Commands from radio to OnStar
	1 11 110011 00100110                 1 000064cf # 3-51-100  Is module present?
	1 11 110011 11111110                 1 00007fcf # 3-51-127  SOURCE button was pressed during OnStar session

	Commands from OnStar to radio
	1 11 001011 0110                     1 000006d3 # 3-52-6     Unmute OnStar audio
	1 11 001011 0111010010               0 00012ed3 # 3-52-46-1  Call established and underway

	Command from IPM to radio
	1 11 100111 0111010XX0               0 00002ee7 # 3-57-46/7-[XX]  Load personalization
	                   ..                     XX    #   XX  memory number 1 or 2
