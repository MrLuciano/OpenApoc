#include "framework/musicloader_interface.h"
#include "framework/framework.h"
#include <algorithm>
#include <vector>
#include <memory>

namespace {

using namespace OpenApoc;

std::vector<unsigned int> starts = { 0, 8467200, 19580400, 35897400, 40131000, 46569600, 57947400, 72147600, 84142800, 92610000, 104076000, 107780400, 118540800, 130712400, 142090200, 154085400, 165904200, 176664600, 187248600, 196686000, 207270000, 218559600, 231436800, 234082800, 237169800, 239815800, 242461800, 245107800, 247842000, 250488000, 263365200, 275801400, 287796600, 298821600, 304819200 };
std::vector<unsigned int> lengths = { 8202600, 19404000, 35897400, 40131000, 46569600, 57859200, 72147600, 84054600, 92610000, 103987800, 107780400, 118452600, 130536000, 142090200, 153997200, 165816000, 176488200, 187072200, 196686000, 207093600, 218383200, 231348600, 234082800, 237169800, 239815800, 242461800, 245107800, 247842000, 250488000, 263188800, 275713200, 287708400, 298821600, 304731000, 311434200 };

MusicTrack::MusicCallbackReturn
fillMusicData(std::shared_ptr<MusicTrack> thisTrack, unsigned int maxSamples, void *sampleBuffer, unsigned int *returnedSamples);

class RawMusicTrack : public MusicTrack
{
	PHYSFS_file *file;
	unsigned int samplePosition;

public:
	RawMusicTrack(PHYSFS_file *file, unsigned int numSamples)
		:file(file)
	{
		this->sampleCount = numSamples;
		this->format.frequency = 22050;
		this->format.channels = 2;
		this->format.format = AudioFormat::SampleFormat::PCM_SINT16;
		this->callback = fillMusicData;

		//Ask for 1/2 a second of buffer by default
		this->requestedSampleBufferSize = this->format.frequency / 2;
	}
	MusicTrack::MusicCallbackReturn fillData(unsigned int maxSamples, void *sampleBuffer, unsigned int *returnedSamples)
	{
		unsigned int samples = std::min(maxSamples, this->sampleCount - this->samplePosition);

		this->samplePosition += samples;

		int bytesPerSample = 2;
		PHYSFS_readBytes(this->file, sampleBuffer, samples * bytesPerSample * this->format.channels);
		*returnedSamples = samples;
		if (samples < maxSamples)
			return MusicTrack::MusicCallbackReturn::End;
		return MusicTrack::MusicCallbackReturn::Continue;
	}

	virtual ~RawMusicTrack()
	{
		PHYSFS_close(file);
	}
};

MusicTrack::MusicCallbackReturn
fillMusicData(std::shared_ptr<MusicTrack> thisTrack, unsigned int maxSamples, void *sampleBuffer, unsigned int *returnedSamples)
{
	auto track = std::dynamic_pointer_cast<RawMusicTrack>(thisTrack);
	assert(track);
	return track->fillData(maxSamples, sampleBuffer, returnedSamples);
}

class RawMusicLoader : public MusicLoader
{
	Framework &fw;
public:
	RawMusicLoader(Framework &fw)
		: fw(fw)
	{
	}
	virtual ~RawMusicLoader()
	{
	}

	virtual std::shared_ptr<MusicTrack> loadMusic(std::string path)
	{
		auto strings = Strings::Split(path, ':');
		if (strings.size() != 2)
		{
			std::cerr << "Invalid raw music path string\n";
			return nullptr;
		}

		if (!Strings::IsNumeric(strings[1]))
		{
			std::cerr << "Raw music track \"" << strings[1] << "\" doesn't look like a number\n";
			return nullptr;
		}

		int track = Strings::ToInteger(strings[1]);
		if (track > lengths.size())
		{
			std::cerr << "Raw music track " << track << " out of bounds\n";
			return nullptr;
		}

		PHYSFS_file *file = fw.data->load_file(strings[0], "r");
		if (!file)
		{
			std::cerr << "Failed to open raw music file \"" << strings[0] << "\"\n";
			return nullptr;
		}

		if (PHYSFS_fileLength(file) < lengths[track] + starts[track])
		{
			std::cerr << "Raw music file of insufficient size\n";
			PHYSFS_close(file);
			return nullptr;
		}
		PHYSFS_seek(file, starts[track]);
		int channels = 2;
		int bytesPerSample = 2;
		return std::make_shared<RawMusicTrack>(file, lengths[track]/channels/bytesPerSample);
	}

};

class RawMusicLoaderFactory : public MusicLoaderFactory
{
public:
	virtual MusicLoader *create(Framework &fw)
	{
		return new RawMusicLoader(fw);
	}
};

MusicLoaderRegister<RawMusicLoaderFactory> load_at_init("raw");

#if 0
Music::Music( Framework &fw, int Track )
{
	PHYSFS_file* f = fw.data->load_file( "MUSIC", "rb" );
	data.reset(new char[lengths[Track]]);
	PHYSFS_seek( f, starts[Track]);
	PHYSFS_readBytes( f, &data[0], lengths[Track] );
	PHYSFS_close( f );

	playing = false;

	soundsample = al_create_sample( &data[0], lengths[Track], 22050, ALLEGRO_AUDIO_DEPTH_INT16, ALLEGRO_CHANNEL_CONF_2, false );
}

Music::~Music()
{
	if (playing)
		al_stop_sample(&play_id);
	if (soundsample)
		al_destroy_sample( soundsample );
}

void Music::Play()
{
	/* If already playing, restart */
	if (playing)
		al_stop_sample(&play_id);
	playing = al_play_sample( soundsample, 1.0f, 0.0f, 1.0f, ALLEGRO_PLAYMODE_ONCE, &play_id );
}

void Music::Stop()
{
	if (playing)
		al_stop_sample(&play_id);
	playing = false;
}
#endif

}; //anonymous namespace
