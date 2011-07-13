/*
 *	sha1.h
 *
 *	Copyright (C) 1998
 *	Paul E. Jones <paulej@arid.us>
 *	All Rights Reserved.
 *
 *****************************************************************************
 *	$Id: sha1.h,v 1.6 2004/03/27 18:02:26 paulej Exp $
 *****************************************************************************
 *
 *	Description:
 * 		This class implements the Secure Hashing Standard as defined
 * 		in FIPS PUB 180-1 published April 17, 1995.
 *
 *		Many of the variable names in this class, especially the single
 *		character names, were used because those were the names used
 *		in the publication.
 *
 * 		Please read the file sha1.cpp for more information.
 *
 */

#ifndef _SHA1_H_
#define _SHA1_H_

#ifdef _WINDOWS
#include <windows.h>
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
#else
#include <stdint.h>
#endif

class SHA1
{

	public:

		SHA1();
		virtual ~SHA1();

		/*
		 *	Re-initialize the class
		 */
		void Reset();

		/*
		 *	Returns the message digest
		 */
		bool Result(uint32_t *message_digest_array);

		/*
		 *	Provide input to SHA1
		 */
		void Input(	const unsigned char	*message_array,
				unsigned	length);
		void Input(	const char	*message_array,
				unsigned	length);
		void Input(unsigned char message_element);
		void Input(char message_element);
		SHA1& operator<<(const char *message_array);
		SHA1& operator<<(const unsigned char *message_array);
		SHA1& operator<<(const char message_element);
		SHA1& operator<<(const unsigned char message_element);

	private:

		/*
		 *	Process the next 512 bits of the message
		 */
		void ProcessMessageBlock();

		/*
		 *	Pads the current message block to 512 bits
		 */
		void PadMessage();

		/*
		 *	Performs a circular left shift operation
		 */
		inline uint32_t CircularShift(int bits, uint32_t word);

		uint32_t H[5];				// Message digest buffers

		uint64_t Length;			// Message length in bits

		unsigned char Message_Block[64];	// 512-bit message blocks
		int Message_Block_Index;		// Index into message block array

		bool Computed;				// Is the digest computed?
		bool Corrupted;				// Is the message digest corruped?
	
};

#endif
