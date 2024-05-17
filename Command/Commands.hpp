#pragma once

namespace twen 
{
	class CommandSignature : public ShareObject<CommandSignature>, public DeviceChild
	{
	public:
		CommandSignature(Device& device, ::std::shared_ptr<RootSignature> rootSignature, 
			::std::vector<::D3D12_INDIRECT_ARGUMENT_DESC> const& args);

		operator::ID3D12CommandSignature* () const { return m_CommandSignature.Get(); }
	public:
		const::UINT CommandCount;
	protected:
		ComPtr<::ID3D12CommandSignature> m_CommandSignature;
	};

	class CommandBundle 
	{

	};
}