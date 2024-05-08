#pragma once

namespace twen 
{
	struct Context;

	struct Internment 
	{
		bool SetFence(ID3D12Fence* fence, ::UINT64 wait) 
		{
			ScopeLock _{ CriticalSection };
			if (Fence) return false;

			Fence = fence;
			Fence->SetEventOnCompletion(wait, nullptr);
			return true;
		}
		bool Register(Context& context) 
		{
			assert(Fence && "Internment should not be availble.");
			ScopeLock _{ CriticalSection };
			if (Queue) return false; // On executed so register failure.

			Consumer.push_back(&context);
			return true;
		}
		bool Close(Queue* queue);

		::ID3D12Fence* Fence;
		Queue* Queue;

		::std::vector<Context*> Consumer;
		CriticalSection CriticalSection;
	};

	struct SyncPoint
	{
		Event Event;
		::ID3D12Fence* Fence;

		void Wait();

	};
}