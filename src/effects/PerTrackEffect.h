/**********************************************************************

  Audacity: A Digital Audio Editor

  PerTrackEffect.h

  Dominic Mazzoni
  Vaughan Johnson

  Paul Licameli split from Effect.h

**********************************************************************/

#ifndef __AUDACITY_PER_TRACK_EFFECT__
#define __AUDACITY_PER_TRACK_EFFECT__

#include "Effect.h" // to inherit
#include "MemoryX.h"
#include <functional>
#include <optional>

#include "SampleFormat.h"

//! Base class for Effects that treat each (mono or stereo) track independently
//! of other tracks.
/*!
   Its override of Effect::Process() uses ProcessInitialize(),
   ProcessBlock(), and ProcessFinalize() methods of its instance made by
   MakeInstance(), which must be a subclass of PerTrackEffect::Instance.
   Also uses GetLatency() to determine how many leading output samples to
   discard and how many extra samples to produce.
 */
class PerTrackEffect
   : public Effect
{
public:
   ~PerTrackEffect() override;

   class AUDACITY_DLL_API Instance : public virtual EffectInstance {
   public:
      explicit Instance(const PerTrackEffect &processor)
         : mProcessor{ processor }
      {}
      ~Instance() override;
   
      //! Uses the other virtual functions of this class
      bool Process(EffectSettings &settings) final;

      //! Called at start of destructive processing, for each (mono/stereo) track
      //! Default implementation does nothing, returns true
      virtual bool ProcessInitialize(EffectSettings &settings,
         double sampleRate, sampleCount totalLen, ChannelNames chanMap);

      //! Called at end of destructive processing, for each (mono/stereo) track
      //! Default implementation does nothing, returns true
      //! This may be called during stack unwinding:
      virtual bool ProcessFinalize() /* noexcept */ ;

      //! Called for destructive effect computation
      virtual size_t ProcessBlock(EffectSettings &settings,
         const float *const *inBlock, float *const *outBlock, size_t blockLen)
      = 0;

      //! Called for destructive, non-realtime effect computation
      //! Default implementation returns zero
      virtual sampleCount GetLatency(
         const EffectSettings &settings, double sampleRate) const;

   protected:
      const PerTrackEffect &mProcessor;
   };

protected:
   // These were overridables but the generality wasn't used yet
   /* virtual */ bool DoPass1() const;
   /* virtual */ bool DoPass2() const;

   // non-virtual
   bool Process(EffectInstance &instance, EffectSettings &settings) const;

   sampleCount    mSampleCnt{};

private:
   //! Accumulates (non-interleaved) data during effect processing
   /*!
    @invariant `mBuffers.size() == mPositions.size()`
    @invariant all `mBuffers[i].size()` are equal to `BufferSize()`
    @invariant all `(mPositions[i] - mBuffers[i].data())` are equal and in
       range [`0`, `BufferSize()`]
    */
   class Buffers {
   public:
      //! Initially empty
      Buffers();
      unsigned Channels() const { return mBuffers.size(); }
      size_t BufferSize() const { return mBufferSize; }
      /*!
       @post `Channels() == nChannels`
       @post `BufferSize() == bufferSize`       
       */
      void Reinit(unsigned nChannels, size_t bufferSize);
      //! Get array of positions in the buffers
      float *const *Positions() const { return mPositions.data(); }
      //! Discard some data at the (unchanging) positions
      /*!
       @param drop how many values to discard
       @param keep how many following values are defined
       */
      void Discard(size_t drop, size_t keep);
      //! Move the positions
      void Advance(size_t count);
      //! Reset positions to starts of buffers
      void Rewind();

      //! Get accumulated data for one channel
      /*!
       Last channel is replicated for all greater indices
       @pre `Channels() > 0`
       @pre `BufferSize() > 0`
       @post result: `result != nullptr`
       */
      constSamplePtr GetReadPosition(unsigned iChannel) const;

      //! Get writable position for one channel
      /*!
       @pre `iChannel < Channels()`
       @pre `BufferSize() > 0`
       */
      float &GetWritePosition(unsigned iChannel);

      //! Zero-fill n places in one of the buffers,
      //! starting from its position plus offset
      void ClearBuffer(unsigned iChannel, size_t n, size_t offset = 0);
   private:
      std::vector<std::vector<float>> mBuffers;
      std::vector<float *> mPositions;
      size_t mBufferSize{ 0 };
   };

   bool ProcessPass(Instance &instance, EffectSettings &settings);
   //! Type of function returning false if user cancels progress
   using Poller = std::function<bool(sampleCount blockSize)>;
   /*!
    @pre `inBuffers.Channels() > 0`
    @pre `inBuffers.BufferSize() > 0`
    @pre `!pRight || inBuffers.Channels() > 1`
    @pre `outBuffers.Channels() > 0`
    @pre `outBuffers.BufferSize() > 0`
    */
   bool ProcessTrack(Instance &instance, EffectSettings &settings,
      const Poller &pollUser, std::optional<sampleCount> genLength,
      double sampleRate, ChannelNames map,
      WaveTrack &left, WaveTrack *pRight,
      sampleCount start, sampleCount len,
      Buffers &inBuffers, Buffers &outBuffers,
      size_t bufferSize, size_t blockSize,
      unsigned mNumChannels) const;
};
#endif
