#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <DataIO.h>
#include <ByteOrder.h>
#include <InterfaceDefs.h>
#include "WavReaderPlugin.h"

#define TRACE_THIS 1
#if TRACE_THIS
  #define TRACE printf
#else
  #define TRACE(a...)
#endif

#define BUFFER_SIZE	16384 // must be > 5200 for mp3 decoder to work

#define FOURCC(a,b,c,d)	((((uint32)(d)) << 24) | (((uint32)(c)) << 16) | (((uint32)(b)) << 8) | ((uint32)(a)))
#define UINT16(a) 		((uint16)B_LENDIAN_TO_HOST_INT16((a)))
#define UINT32(a) 		((uint32)B_LENDIAN_TO_HOST_INT32((a)))

struct wavdata
{
	uint64 position;
	uint64 datasize;

	int32 bitsperframe;
	int32 fps;

	void *buffer;
	int32 buffersize;
	
	int64 framecount;
	bigtime_t duration;
	
	bool raw;
	
	media_format format;
};

WavReader::WavReader()
{
	TRACE("WavReader::WavReader\n");
}

WavReader::~WavReader()
{
}
      
const char *
WavReader::Copyright()
{
	return "WAV reader, " B_UTF8_COPYRIGHT " by Marcus Overhagen";
}

	
status_t
WavReader::Sniff(int32 *streamCount)
{
	TRACE("WavReader::Sniff\n");

	fSource = dynamic_cast<BPositionIO *>(Reader::Source());

	fFileSize = Source()->Seek(0, SEEK_END);
	if (fFileSize < 44) {
		TRACE("WavReader::Sniff: File too small\n");
		return B_ERROR;
	}
	
	int64 pos = 0;
	
	riff_struct riff;

	if (sizeof(riff) != Source()->ReadAt(pos, &riff, sizeof(riff))) {
		TRACE("WavReader::Sniff: RIFF WAVE header reading failed\n");
		return B_ERROR;
	}
	pos += sizeof(riff);

	if (UINT32(riff.riff_id) != FOURCC('R','I','F','F') || UINT32(riff.wave_id) != FOURCC('W','A','V','E')) {
		TRACE("WavReader::Sniff: RIFF WAVE header not recognized\n");
		return B_ERROR;
	}
	
	format_struct format;
	format_struct_extensible format_ext;
	fact_struct fact;
	
	// read all chunks and search for "fact", "fmt" (normal or extensible) and "data"
	// everything else is ignored;
	bool foundFact = false;
	bool foundFmt = false;
	bool foundFmtExt = false;
	bool foundData = false;
	while (pos + sizeof(chunk_struct) <= fFileSize) {
		chunk_struct chunk;
		if (sizeof(chunk) != Source()->ReadAt(pos, &chunk, sizeof(chunk))) {
			TRACE("WavReader::Sniff: chunk header reading failed\n");
			return B_ERROR;
		}
		pos += sizeof(chunk);
		switch (UINT32(chunk.fourcc)) {
			case FOURCC('f','m','t',' '):
				if (UINT32(chunk.len) >= sizeof(format)) {
					if (sizeof(format) != Source()->ReadAt(pos, &format, sizeof(format))) {
						TRACE("WavReader::Sniff: format chunk reading failed\n");
						break;
					}
					foundFmt = true;
					if (UINT32(chunk.len) >= sizeof(format_ext) && UINT16(format.format_tag) == 0xfffe) {
						if (sizeof(format_ext) != Source()->ReadAt(pos, &format_ext, sizeof(format_ext))) {
							TRACE("WavReader::Sniff: format extensible chunk reading failed\n");
							break;
						}
						foundFmtExt = true;
					}
				}
				break;
			case FOURCC('f','a','c','t'):
				if (UINT32(chunk.len) >= sizeof(fact)) {
					if (sizeof(fact) != Source()->ReadAt(pos, &fact, sizeof(fact))) {
						TRACE("WavReader::Sniff: fact chunk reading failed\n");
						break;
					}
					foundFact = true;
				}
				break;
			case FOURCC('d','a','t','a'):
				fDataStart = pos;
				fDataSize = UINT32(chunk.len);
				foundData = true;
				// If file size is >= 2GB and we already found a format chunk,
				// assume the rest of the file is data and get out of here.
				// This should allow reading wav files much bigger than 2 or 4 GB.
				if (fFileSize >= 0x7fffffff && foundFmt) {
					pos += fFileSize;
					fDataSize = fFileSize - fDataStart;
					TRACE("WavReader::Sniff: big file size %Ld, indicated data size %lu, assuming data size is %Ld\n",
						fFileSize, UINT32(chunk.len), fDataSize);
				}
				break;
			default:
				TRACE("WavReader::Sniff: ignoring chunk 0x%08lx of %lu bytes\n", UINT32(chunk.fourcc), chunk.len);
				break;
		}
		pos += UINT32(chunk.len);
		pos += (pos & 1);
	}

	if (!foundFmt) {
		TRACE("WavReader::Sniff: couldn't find format chunk\n");
		return B_ERROR;
	}
	if (!foundData) {
		TRACE("WavReader::Sniff: couldn't find data chunk\n");
		return B_ERROR;
	}
	
	TRACE("WavReader::Sniff: we found something that looks like:\n");
	
	TRACE("  format_tag            0x%04x\n", UINT16(format.format_tag));
	TRACE("  channels              %d\n", UINT16(format.channels));
	TRACE("  samples_per_sec       %ld\n", UINT32(format.samples_per_sec));
	TRACE("  avg_bytes_per_sec     %ld\n", UINT32(format.avg_bytes_per_sec));
	TRACE("  block_align           %d\n", UINT16(format.block_align));
	TRACE("  bits_per_sample       %d\n", UINT16(format.bits_per_sample));
	if (foundFmtExt) {
		TRACE("  ext_size              %d\n", UINT16(format_ext.ext_size));
		TRACE("  valid_bits_per_sample %d\n", UINT16(format_ext.valid_bits_per_sample));
		TRACE("  channel_mask          %ld\n", UINT32(format_ext.channel_mask));
		TRACE("  guid[0-1] format      0x%04x\n", (format_ext.guid[1] << 8) | format_ext.guid[0]);
	}
	if (foundFact) {
		TRACE("  sample_length         %ld\n", UINT32(fact.sample_length));
	}

	fChannelCount = UINT16(format.channels);
	fFrameRate = UINT32(format.samples_per_sec);
	fBitsPerSample = UINT16(format.bits_per_sample);
	if (fBitsPerSample == 0) {
		printf("WavReader::Sniff: Error, bits_per_sample = 0\n");
		fBitsPerSample = 8;
	}
	fFrameCount = foundFact ? UINT32(fact.sample_length) : 0;
	fFormatCode = UINT16(format.format_tag);
	if (fFormatCode == 0xfffe && foundFmtExt)
		fFormatCode = (format_ext.guid[1] << 8) | format_ext.guid[0];

	fBlockAlign = UINT16(format.block_align);
	int min_align = (fFormatCode == 0x0001) ? (fBitsPerSample * fChannelCount + 7) / 8 : 1;
	if (fBlockAlign < min_align)
		fBlockAlign = min_align;

	TRACE("  fDataStart     %Ld\n", fDataStart);
	TRACE("  fDataSize      %Ld\n", fDataSize);
	TRACE("  fChannelCount  %ld\n", fChannelCount);
	TRACE("  fFrameRate     %ld\n", fFrameRate);
	TRACE("  fFrameCount    %Ld\n", fFrameCount);
	TRACE("  fBitsPerSample %d\n", fBitsPerSample);
	TRACE("  fBlockAlign    %d\n", fBlockAlign);
	TRACE("  min_align      %d\n", min_align);
	TRACE("  fFormatCode    0x%04x\n", fFormatCode);
	
	// XXX fact.sample_length contains duration of encodec files?

	*streamCount = 1;
	return B_OK;
}


void
WavReader::GetFileFormatInfo(media_file_format *mff)
{
	mff->capabilities =   media_file_format::B_READABLE
						| media_file_format::B_KNOWS_ENCODED_AUDIO
						| media_file_format::B_IMPERFECTLY_SEEKABLE;
	mff->family = B_WAV_FORMAT_FAMILY;
	mff->version = 100;
	strcpy(mff->mime_type, "audio/x-wav");
	strcpy(mff->file_extension, "wav");
	strcpy(mff->short_name,  "RIFF WAV audio");
	strcpy(mff->pretty_name, "RIFF WAV audio");
}


status_t
WavReader::AllocateCookie(int32 streamNumber, void **cookie)
{
	TRACE("WavReader::AllocateCookie\n");

	wavdata *data = new wavdata;

	data->position = 0;
	data->datasize = fDataSize;
	data->bitsperframe = fChannelCount * fBitsPerSample;
	data->fps = fFrameRate;
	data->buffersize = (BUFFER_SIZE / fBlockAlign) * fBlockAlign;
	data->buffer = malloc(data->buffersize);
	data->framecount = fFrameCount ? fFrameCount : (8 * fDataSize) / data->bitsperframe;
	data->duration = (data->framecount * 1000000LL) / data->fps;
	data->raw = fFormatCode == 0x0001;

	TRACE(" bitsperframe %ld\n", data->bitsperframe);
	TRACE(" fps %ld\n", data->fps);
	TRACE(" buffersize %ld\n", data->buffersize);
	TRACE(" framecount %Ld\n", data->framecount);
	TRACE(" duration %Ld\n", data->duration);
	TRACE(" raw %d\n", data->raw);
	
	BMediaFormats formats;
	if (fFormatCode == 0x0001) {
		// a raw PCM format
		media_format_description description;
		description.family = B_BEOS_FORMAT_FAMILY;
		description.u.beos.format = B_BEOS_FORMAT_RAW_AUDIO;
		formats.GetFormatFor(description, &data->format);
		data->format.u.raw_audio.frame_rate = fFrameRate;
		data->format.u.raw_audio.channel_count = fChannelCount;
		switch (fBitsPerSample) {
			case 8:
				data->format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_UCHAR;
				break;
			case 16:
				data->format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_SHORT;
				break;
			case 24:
				data->format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_INT;
				break;
			case 32:
				data->format.u.raw_audio.format = media_raw_audio_format::B_AUDIO_INT;
				break;
			default:
				TRACE("WavReader::AllocateCookie: unhandled bits per sample %d\n", fBitsPerSample);
				return B_ERROR;
		}
		data->format.u.raw_audio.byte_order = B_MEDIA_LITTLE_ENDIAN;
		data->format.u.raw_audio.buffer_size = data->buffersize;
	} else {
		// some encoded format
		media_format_description description;
		description.family = B_WAV_FORMAT_FAMILY;
		description.u.wav.codec = fFormatCode;
		formats.GetFormatFor(description, &data->format);
		data->format.u.encoded_audio.output.frame_rate = fFrameRate;
		data->format.u.encoded_audio.output.channel_count = fChannelCount;
	}
	
	// store the cookie
	*cookie = data;
	return B_OK;
}


status_t
WavReader::FreeCookie(void *cookie)
{
	TRACE("WavReader::FreeCookie\n");
	wavdata *data = reinterpret_cast<wavdata *>(cookie);

	free(data->buffer);
	delete data;
	
	return B_OK;
}


status_t
WavReader::GetStreamInfo(void *cookie, int64 *frameCount, bigtime_t *duration,
						 media_format *format, void **infoBuffer, int32 *infoSize)
{
	wavdata *data = reinterpret_cast<wavdata *>(cookie);
	
	*frameCount = data->framecount;
	*duration = data->duration;
	*format = data->format;
	*infoBuffer = 0;
	*infoSize = 0;
	return B_OK;
}


status_t
WavReader::Seek(void *cookie,
				uint32 seekTo,
				int64 *frame, bigtime_t *time)
{
	wavdata *data = reinterpret_cast<wavdata *>(cookie);
	uint64 pos;

	if (seekTo & B_MEDIA_SEEK_TO_FRAME) {
		if (data->raw)
			pos = (*frame * data->bitsperframe) / 8;
		else
			pos = (*frame * fDataSize) / data->framecount;
		pos = (pos / fBlockAlign) * fBlockAlign; // round down to a block start
		TRACE("WavReader::Seek to frame %Ld, pos %Ld\n", *frame, pos);
	} else if (seekTo & B_MEDIA_SEEK_TO_TIME) {
		if (data->raw)
			pos = (*time * data->fps * data->bitsperframe) / (1000000LL * 8);
		else
			pos = (*time * fDataSize) / data->duration;
		pos = (pos / fBlockAlign) * fBlockAlign; // round down to a block start
		TRACE("WavReader::Seek to time %Ld, pos %Ld\n", *time, pos);
	} else {
		return B_ERROR;
	}

	if (data->raw)
		*frame = (8 * pos) / data->bitsperframe;
	else
		*frame = (pos * data->framecount) / fDataSize;
	*time = (*frame * 1000000LL) / data->fps;

	TRACE("WavReader::Seek newtime %Ld\n", *time);
	TRACE("WavReader::Seek newframe %Ld\n", *frame);
	
	if (pos < 0 || pos > data->datasize) {
		TRACE("WavReader::Seek invalid position %Ld\n", pos);
		return B_ERROR;
	}
	
	data->position = pos;
	return B_OK;
}


status_t
WavReader::GetNextChunk(void *cookie,
						void **chunkBuffer, int32 *chunkSize,
						media_header *mediaHeader)
{
	wavdata *data = reinterpret_cast<wavdata *>(cookie);

	// XXX it might be much better to not return any start_time information for encoded formats here,
	// XXX and instead use the last time returned from seek and count forward after decoding.
	if (data->raw)
		mediaHeader->start_time = (((8 * data->position) / data->bitsperframe) * 1000000LL) / data->fps;
	else
		mediaHeader->start_time = (data->position * data->duration) / fDataSize;
	mediaHeader->file_pos = fDataStart + data->position;

/*
	printf("position   = %9Ld\n", data->position);
	printf("fDataSize  = %9Ld\n", fDataSize);
	printf("duration   = %9Ld\n", data->duration);
	printf("start_time = %9Ld\n", mediaHeader->start_time);
*/	

	int64 maxreadsize = data->datasize - data->position;
	int32 readsize = data->buffersize;
	if (maxreadsize < readsize)
		readsize = maxreadsize;
	if (readsize == 0)
		return B_LAST_BUFFER_ERROR;
		
	if (readsize != Source()->ReadAt(fDataStart + data->position, data->buffer, readsize)) {
		TRACE("WavReader::GetNextChunk: unexpected read error\n");
		return B_ERROR;
	}
	
	// XXX if the stream has more than two channels, we need to reorder channel data here
	
	data->position += readsize;
	*chunkBuffer = data->buffer;
	*chunkSize = readsize;
	return B_OK;
}


Reader *
WavReaderPlugin::NewReader()
{
	return new WavReader;
}


MediaPlugin *instantiate_plugin()
{
	return new WavReaderPlugin;
}
