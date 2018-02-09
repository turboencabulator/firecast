/*
 * Firecast:  Cassette deck emulator and E&C Bus communicator for GM radios
 * Copyright 2018 Kyle Guinn
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/*
 * Commands from the head unit (or best guesses as to their meaning):
 * Lower 8 bits of 0x73 signify a command to the cassette deck.
 *
 * If the cassette deck doesn't respond correctly before some timeout, the
 * head unit appears to retry sending the command (twice, for a total of 3
 * tries; the timeout seems to vary per command).  If after the final try and
 * timeout the head unit is still not happy with the response, it sends the
 * eject command.
 */
#define CMD_POLL       0x00001073u
#define CMD_EJECT      0x00000473u
#define CMD_STOP       0x00000273u
#define CMD_PLAY_F     0x00003c73u
#define CMD_PLAY_R     0x00001c73u
#define CMD_NEXT       0x00003473u
#define CMD_PREV       0x00001473u
#define CMD_FAST_F     0x00002c73u
#define CMD_FAST_R     0x00000c73u
#define CMD_DOLBY_NR_1 0x00002873u
#define CMD_DOLBY_NR_0 0x00003073u

/*
 * Best guesses as to what bit means what in the responses:
 * Lower 8 bits of 0x61 signify a response from the cassette deck.
 *
 * Appears to be two status registers (distinguished by bit 10), where each
 * register is sent upon any of its bits changing (maybe?), or upon request.
 */
#define STAT0          0x00000061u
#define STAT0_PLAY_F   0x00000800u
#define STAT0_PLAY_R   0x00001000u
#define STAT0_DOLBY_NR 0x00004000u
#define STAT0_TAPE_IN  0x00008000u
#define STAT0_READY    0x00010000u
#define STAT0_STOP     0x00040000u

#define STAT1          0x00000461u
#define STAT1_LOADED   0x00000800u
#define STAT1_SEEK     0x00002000u
#define STAT1_FLIP     0x00004000u
#define STAT1_UNLOADED 0x00008000u
#define STAT1_FAST_F   0x00010000u
#define STAT1_FAST_R   0x00020000u
#define STAT1_TAPE_IN  0x00080000u

/*
 * Serializes/Deserializes an E&C Bus frame:
 *   1 start bit (always 1)
 *   Up to 32 data bits (see defined values above)
 *     Sent/received LSb first.
 *     Most-significant (trailing) zeros don't need to appear on the bus,
 *     however the number of data bits that appear on the bus must be even.
 *   1 parity bit (1 if data contains an odd number of 1s, otherwise 0)
 */
class ECFrame {
	uint32_t data;
	int nbits;
	bool parity = true;
	bool hold = true;
	bool bad = false;

public:
	/* Init for RX/shiftIn */
	ECFrame(): data(0), nbits(-2) { }
	/* Init for TX/shiftOut */
	ECFrame(uint32_t data): data(data), nbits(32) { }

	/*
	 * Shifts the frame in, one bit at a time.
	 *
	 * There is no good way to determine if a frame has been fully
	 * received (since checking isValid after every bit may give
	 * false-positives); a better method is to wait for the bus to go
	 * idle.  I do not know the exact timing specifications, but 3-5 bit
	 * times (3-5 ms) seems to work OK.
	 */
	void shiftIn(bool b)
	{
		if (bad
		 || nbits >= 32          // too long
		 || nbits == -2 && !b) { // bad start bit
			bad = true;
		} else {
			if (nbits >= 0) {
				data |= (uint32_t)hold << nbits;
			}
			++nbits;
			parity ^= b;
			hold = b;
		}
	}
	bool isValid() const
	{
		return !bad && !parity && nbits >= 0 && !(nbits & 1);
	}
	uint32_t getData() const
	{
		return data;
	}

	/*
	 * Shifts the frame out, one bit at a time.
	 *
	 * nbits will be -2 after shifting out the parity bit.  Stop then:
	 *   while (!frame.isEmpty()) { tx_bit(frame.shiftOut()); }
	 *
	 * If data is 0, it will send start + 2 zero bits + parity bit (0).
	 * This could be shortened to start + parity, but a zero-data frame
	 * probably isn't valid to begin with.
	 */
	bool shiftOut()
	{
		bool out = hold;
		parity ^= out;
		--nbits;
		if (nbits >= 0) {
			hold = data & 1;
			data >>= 1;
			if (!(nbits & 1) && !data) {
				nbits = 0;
			}
		} else {
			hold = parity;
		}
		return out;
	}
	bool isEmpty() const
	{
		return nbits <= -2;
	}
};

/*
 * Simple queue for 32-bit data words.  Serves two purposes when used as the
 * TX queue:  Allows multiple words to be queued for later transmission, and
 * allows for a TX to be aborted and later retried in case of bus contention.
 */
class RingBuffer {
	uint8_t rd = 0;
	uint8_t wr = 0;
#define RINGBUF_LEN 16
	uint32_t buf[RINGBUF_LEN];

public:
	bool isEmpty() const
	{
		return rd == wr;
	}
	bool isFull() const
	{
		return rd == (wr + 1) % RINGBUF_LEN;
	}

	/*
	 * Primitive operations
	 * Always do an appropriate isEmpty/isFull check before calling these.
	 */
	void push(uint32_t data)
	{
		buf[wr] = data;
		wr = (wr + 1) % RINGBUF_LEN;
	}
	uint32_t peek() const
	{
		return buf[rd];
	}
	void pop()
	{
		rd = (rd + 1) % RINGBUF_LEN;
	}

	/*
	 * Advanced operations
	 */
	void enqueue(uint32_t data)
	{
		if (!isFull()) {
			push(data);
		}
	}
};

/*
 * Basic idea is to use the UART to generate and read the E&C Bus waveforms:
 *   8N1, 10000 baud:
 *     Each UART bit takes 0.1 ms to send.
 *     Each UART byte takes 1 ms to send (0.1 ms * (start + 8 data + stop))
 *   Start bit initially pulls the bus low for 0.1 ms.
 *   Value written to the UART can cause the bus to be pulled low for longer.
 *   Send an E&C '1' by writing 0xe0, send a '0' by writing 0xff.  See below.
 *   To receive, extract UART bit 2 (since it's about in the middle of the low
 *   period for an E&C '1') and invert it.
 *
 *     Start 0   1   2   3   4   5   6   7 Stop
 *     |<------           1 ms          ------>|
 *   __     _____________________________________  E&C '0' == UART 0xff
 *     \___/ 1   1   1   1   1   1   1   1     \_  pulled low for 0.1 ms
 *   __                         _________________  E&C '1' == UART 0xe0
 *     \_____0___0___0___0___0_/ 1   1   1     \_  pulled low for 0.6 ms
 *
 * I do not know the exact timing specifications.  The parameters can be
 * adjusted to get different bit timings, e.g. 9000 7N1 or 11000 8N2.
 *
 * For this to work, the glue logic to connect the UART to the E&C Bus must be
 * non-inverting.  It must also let the bus float high instead of driving it
 * high, so that other devices can pull the bus low.
 */
#define EC_UART  Serial
#define EC_BAUD  10000
#define EC_FRAME SERIAL_8N1

#define EC_0     0xff // bit pattern to send a '0'
#define EC_1     0xe0 // bit pattern to send a '1'
#define EC_MASK  0x04 // bit mask for sampling input

#define EC_IDLE  5    // ms; process RX'd frame if bus goes idle for this long

/*
 * Since the bus is shared, we need to do carrier sense/collision detection.
 * Don't start a new TX if something else is currently TXing.  Without any
 * good way to peek inside the UART to see if there is an RX in progress, wire
 * the bus to an interrupt-capable pin and watch for incoming data.
 *
 * If the time since the last falling edge is less than 1 bit time (about 1000
 * us), the UART RXer should be busy, or we picked up some noise.  At or
 * beyond 1 bit time, the UART RX complete interrupt should fire, the RX data
 * gets moved to a FIFO, and we can check the FIFO depth.  Add some margin to
 * account for interrupt latency, but it should be less than the EC_IDLE time.
 */
#define EC_SENSE 2     // digital interrupt-capable pin
#define EC_BUSY  1500u // us; busy if ec_usec_since_fall is less than this
volatile unsigned long last_fall;
void ec_fall_isr()
{
	last_fall = micros();
}
unsigned long ec_usec_since_fall()
{
	unsigned long then, now;

	// XXX:  Expects interrupts to be enabled at the time of the call.
	noInterrupts();
	then = last_fall;
	interrupts();
	now = micros();
	return now - then;
}

ECFrame inframe;
ECFrame outframe;
RingBuffer outqueue;
uint32_t stat[2];

void setup()
{
	stat[0] = STAT0 | STAT0_DOLBY_NR;
	stat[1] = STAT1 | STAT1_UNLOADED;

	// Insert tape.
	stat[0] |=  (STAT0_TAPE_IN | STAT0_READY | STAT0_STOP);
	stat[1] |=  (STAT1_TAPE_IN | STAT1_LOADED);
	stat[1] &= ~(STAT1_UNLOADED);
	outqueue.enqueue(stat[1]);
	// This isn't immediately sent; the main loop waits for at least 1
	// EC_IDLE period to ensure the bus is not busy.

	// Start communicating.
	last_fall = micros();
	pinMode(EC_SENSE, INPUT);
	attachInterrupt(digitalPinToInterrupt(EC_SENSE), ec_fall_isr, FALLING);

	EC_UART.begin(EC_BAUD, EC_FRAME);
	EC_UART.setTimeout(EC_IDLE);
}

void loop()
{
	uint8_t txc = 0, rxc;

	// TX one E&C bit if the TXer is loaded and the bus isn't busy.
	if (!outframe.isEmpty()) {
		if (digitalRead(EC_SENSE) == LOW) {
			// Something else is pulling the bus low before we are
			// about to; abort this TX frame.
			outframe = ECFrame();
		} else {
			txc = outframe.shiftOut() ? EC_1 : EC_0;
			EC_UART.write(txc);
		}
	}
	// Block until one E&C bit is RX'd, or until the EC_IDLE timeout.
	if (EC_UART.readBytes(&rxc, 1)) {
		// Got a bit.  If we're TXing, it should be the TX'd bit.
		inframe.shiftIn(!(rxc & EC_MASK));
		if (txc) {
			if (txc != rxc) {
				// Possible collision; abort this TX frame,
				// but keep processing the RX frame.  Two
				// devices may simultaneously pull the bus
				// low, and whoever sends a 0 first loses,
				// assuming contention is resolved similarly
				// to that for the I2C bus (for example).
				outframe = ECFrame();
			} else if (outframe.isEmpty()) {
				// Last bit of the TX frame was successfully
				// RX'd; don't need to process it as an
				// incoming RX frame from some other device.
				inframe = ECFrame();
				outqueue.pop();
				// Don't immediately reload the TXer, but
				// begin a quiet time of at least 1 EC_IDLE
				// period so that other devices can notice the
				// end of the frame and potentially send a
				// response.
			}
		}
		// Begin next loop iteration.
		return;
	}

	// Should only get here when the bus is idle (no bits RX'd within the
	// past EC_IDLE time).  Consider frames as complete and process them.
	if (inframe.isValid()) {
		// Process commands to the cassette deck.  An actual cassette
		// deck appears to be more chatty, but my head unit is fine
		// with this minimal amount of feedback.
		uint32_t data = inframe.getData();
		if (data == CMD_POLL) {
			outqueue.enqueue(stat[1]);

		} else if (data == CMD_EJECT) {
			stat[0] &= ~(STAT0_TAPE_IN | STAT0_READY | STAT0_STOP | STAT0_PLAY_F | STAT0_PLAY_R);
			stat[1] &= ~(STAT1_TAPE_IN | STAT1_LOADED);
			stat[1] |=  (STAT1_UNLOADED);
			outqueue.enqueue(stat[1]);

		} else if (data == CMD_STOP) {
			stat[0] &= ~(STAT0_STOP | STAT0_PLAY_F | STAT0_PLAY_R);
			stat[0] |=  (STAT0_STOP);
			outqueue.enqueue(stat[0]);

		} else if (data == CMD_PLAY_F) {
			stat[0] &= ~(STAT0_STOP | STAT0_PLAY_F | STAT0_PLAY_R);
			stat[0] |=  (STAT0_PLAY_F);
			outqueue.enqueue(stat[0]);

		} else if (data == CMD_PLAY_R) {
			stat[0] &= ~(STAT0_STOP | STAT0_PLAY_F | STAT0_PLAY_R);
			stat[0] |=  (STAT0_PLAY_R);
			outqueue.enqueue(stat[0]);

		} else if (data == CMD_NEXT) {
			stat[0] &= ~(STAT0_STOP | STAT0_PLAY_F | STAT0_PLAY_R);
			outqueue.enqueue(stat[1] | STAT1_FAST_F | STAT1_SEEK);

		} else if (data == CMD_PREV) {
			stat[0] &= ~(STAT0_STOP | STAT0_PLAY_F | STAT0_PLAY_R);
			outqueue.enqueue(stat[1] | STAT1_FAST_R | STAT1_SEEK);

		} else if (data == CMD_FAST_F) {
			stat[0] &= ~(STAT0_STOP | STAT0_PLAY_F | STAT0_PLAY_R);
			outqueue.enqueue(stat[1] | STAT1_FAST_F);

		} else if (data == CMD_FAST_R) {
			stat[0] &= ~(STAT0_STOP | STAT0_PLAY_F | STAT0_PLAY_R);
			outqueue.enqueue(stat[1] | STAT1_FAST_R);

		} else if (data == CMD_DOLBY_NR_1) {
			stat[0] |=  (STAT0_DOLBY_NR);

		} else if (data == CMD_DOLBY_NR_0) {
			stat[0] &= ~(STAT0_DOLBY_NR);

		}
	}

	// RX'd frame has been processed; reset the RXer.
	// Load the TXer if we have something to send and the bus isn't busy.
	// The incoming frame processing above may take a while; we may have
	// received RX data during that time that needs to be processed.
	inframe = ECFrame();
	if (!outqueue.isEmpty()                // have something to send
	 && !EC_UART.available()               // nothing has arrived yet
	 && ec_usec_since_fall() >= EC_BUSY) { // nothing currently arriving
		outframe = ECFrame(outqueue.peek());
	}
}
