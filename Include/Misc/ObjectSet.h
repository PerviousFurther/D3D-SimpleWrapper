#pragma once

namespace twen 
{
	template<typename ChildType>
	struct SetManager
	{
	public:
		using Backing = ChildType;
	public:
		SetManager(::UINT count)
			: NumsElements{count} 
		{ m_FreeBlocks.emplace(0, count); }
	public:
		const::UINT NumsElements;
	public:
		Pointer<Backing> Obtain(::UINT desiredSize)
		{
			if (desiredSize == 0) return{};
			for (auto const [offset, size] : m_FreeBlocks)
			{
				if (size > desiredSize)
				{
					m_FreeBlocks.emplace(offset + desiredSize, size - desiredSize);
					m_FreeBlocks.erase(offset);

					return static_cast<ChildType*>(this)->AddressOf(offset, desiredSize);
				}
			}
			return{};
		}
		::UINT Resize(Pointer<Backing>& pointer, ::UINT desiredCount) 
		{
			auto position = pointer.Offset + pointer.Capability;

			auto it = m_FreeBlocks.upper_bound(position);

			if (it == m_FreeBlocks.end()) return 0u;

			auto const [offset, size] { *it };
			if (offset == position)
			{
				m_FreeBlocks.erase(offset);

				if (size >= desiredCount)
				{
					pointer.Capability += desiredCount;
					// still have space. emplace new space.
					m_FreeBlocks.emplace(offset + desiredCount, size - desiredCount);
				} // new range filled the gap, no need to emplace new space.
				else pointer.Capability += (desiredCount = size);

				return desiredCount;
			}

			return 0u;
		}
		void Collect(Pointer<Backing> const& pointer)
		{
			::UINT offset = pointer.Offset;
			::UINT size = pointer.Capability;

			assert(!m_FreeBlocks.contains(offset) && "Wrong place.");

			auto front = m_FreeBlocks.lower_bound(offset);
			auto back = m_FreeBlocks.empty() ? m_FreeBlocks.begin() : front--;
			while (back != m_FreeBlocks.end() && back->first == offset + size)
			{
				size += back->second;
				m_FreeBlocks.erase(back++);
			}
			while (front != m_FreeBlocks.end() && front->first + front->second == offset)
			{
				offset = front->first;
				size += front->second;
				m_FreeBlocks.erase(front--);
			}
			m_FreeBlocks.emplace(offset, size);

			if constexpr (requires(ChildType b) { b.DiscardAt(pointer); })
				static_cast<ChildType*>(this)->DiscardAt(pointer);
		}
	private:
		::std::map<::UINT, ::UINT> m_FreeBlocks;

		::std::shared_ptr<Backing> m_Backing;
	};
}