#include "WrappedDevice.h"

#include <ShlObj.h>

#include <array>
#include <filesystem>
#include <utility>

#include "shaders/Decals.h"
#include "shaders/Paint.h"

HRESULT WINAPI D3D11CreateDevice_Export(IDXGIAdapter* pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
	const D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, ID3D11Device** ppDevice,
	D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
{
	wil::assign_null_to_opt_param(ppDevice);
	wil::assign_null_to_opt_param(ppImmediateContext);

	// Try to load a real d3d11.dll, D3D11Device will claim ownership of its reference if created successfully
	wil::unique_cotaskmem_string system32Path;
	if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_System, KF_FLAG_DEFAULT, nullptr, system32Path.put())))
	{
		wil::unique_hmodule d3dModule(LoadLibraryW((std::filesystem::path(system32Path.get()) / L"d3d11.dll").c_str()));
		if (d3dModule)
		{
			PFN_D3D11_CREATE_DEVICE createFn = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(d3dModule.get(), "D3D11CreateDevice"));
			if (createFn != nullptr)
			{
				wil::com_ptr_nothrow<ID3D11Device> device;
				HRESULT hr = createFn(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, device.put(), pFeatureLevel, ppImmediateContext);
				if (SUCCEEDED(hr))
				{
					wil::com_ptr_nothrow<ID3D11Device> wrappedDevice = Microsoft::WRL::Make<D3D11Device>(std::move(d3dModule), std::move(device));
					wil::detach_to_opt_param(ppDevice, wrappedDevice);
				}
				return hr;
			}
		}
	}

	return DXGI_ERROR_SDK_COMPONENT_MISSING; // If either LoadLibrary or GetProcAddress fails
}

// ====================================================

D3D11Device::D3D11Device(wil::unique_hmodule module, wil::com_ptr_nothrow<ID3D11Device> device)
	: m_d3dModule(std::move(module), device), m_orig(std::move(device))
{
}

HRESULT STDMETHODCALLTYPE D3D11Device::QueryInterface(REFIID riid, void** ppvObject)
{
	HRESULT hr = __super::QueryInterface(riid, ppvObject);
	if (FAILED(hr))
	{
		hr = m_orig.query_to(riid, ppvObject);
	}
	return hr;
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateBuffer(const D3D11_BUFFER_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Buffer** ppBuffer)
{
	return m_orig->CreateBuffer(pDesc, pInitialData, ppBuffer);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture1D(const D3D11_TEXTURE1D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture1D** ppTexture1D)
{
	return m_orig->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture2D** ppTexture2D)
{
	return m_orig->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateTexture3D(const D3D11_TEXTURE3D_DESC* pDesc, const D3D11_SUBRESOURCE_DATA* pInitialData, ID3D11Texture3D** ppTexture3D)
{
	return m_orig->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateShaderResourceView(ID3D11Resource* pResource, const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc, ID3D11ShaderResourceView** ppSRView)
{
	return m_orig->CreateShaderResourceView(pResource, pDesc, ppSRView);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateUnorderedAccessView(ID3D11Resource* pResource, const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc, ID3D11UnorderedAccessView** ppUAView)
{
	return m_orig->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateRenderTargetView(ID3D11Resource* pResource, const D3D11_RENDER_TARGET_VIEW_DESC* pDesc, ID3D11RenderTargetView** ppRTView)
{
	return m_orig->CreateRenderTargetView(pResource, pDesc, ppRTView);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilView(ID3D11Resource* pResource, const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc, ID3D11DepthStencilView** ppDepthStencilView)
{
	return m_orig->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs, UINT NumElements, const void* pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength, ID3D11InputLayout** ppInputLayout)
{
	return m_orig->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateVertexShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11VertexShader** ppVertexShader)
{
	if (BytecodeLength >= 4 + 16)
	{
		static constexpr std::array<uint32_t, 4> decalShader = { 0x428ffe45, 0x1c518347, 0xb48bed82, 0x25dc2319 };
		static constexpr std::array<uint32_t, 4> paintShader = { 0xfae61074, 0xefbc29b0, 0xa9ec5152, 0x837d0756 };

		const uint32_t* hash = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(pShaderBytecode) + 4);
		if (std::equal(decalShader.begin(), decalShader.end(), hash))
		{
			return m_orig->CreateVertexShader(FIXED_DECAL_SHADER, sizeof(FIXED_DECAL_SHADER), pClassLinkage, ppVertexShader);
		}
		else if (std::equal(paintShader.begin(), paintShader.end(), hash))
		{
			return m_orig->CreateVertexShader(FIXED_PAINT_SHADER, sizeof(FIXED_PAINT_SHADER), pClassLinkage, ppVertexShader);
		}
	}
	return m_orig->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)
{
	return m_orig->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateGeometryShaderWithStreamOutput(const void* pShaderBytecode, SIZE_T BytecodeLength, const D3D11_SO_DECLARATION_ENTRY* pSODeclaration, UINT NumEntries, const UINT* pBufferStrides, UINT NumStrides, UINT RasterizedStream, ID3D11ClassLinkage* pClassLinkage, ID3D11GeometryShader** ppGeometryShader)
{
	return m_orig->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreatePixelShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11PixelShader** ppPixelShader)
{
	return m_orig->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateHullShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11HullShader** ppHullShader)
{
	return m_orig->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDomainShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11DomainShader** ppDomainShader)
{
	return m_orig->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateComputeShader(const void* pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage* pClassLinkage, ID3D11ComputeShader** ppComputeShader)
{
	return m_orig->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateClassLinkage(ID3D11ClassLinkage** ppLinkage)
{
	return m_orig->CreateClassLinkage(ppLinkage);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateBlendState(const D3D11_BLEND_DESC* pBlendStateDesc, ID3D11BlendState** ppBlendState)
{
	return m_orig->CreateBlendState(pBlendStateDesc, ppBlendState);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc, ID3D11DepthStencilState** ppDepthStencilState)
{
	return m_orig->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateRasterizerState(const D3D11_RASTERIZER_DESC* pRasterizerDesc, ID3D11RasterizerState** ppRasterizerState)
{
	return m_orig->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc, ID3D11SamplerState** ppSamplerState)
{
	return m_orig->CreateSamplerState(pSamplerDesc, ppSamplerState);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateQuery(const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery)
{
	return m_orig->CreateQuery(pQueryDesc, ppQuery);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreatePredicate(const D3D11_QUERY_DESC* pPredicateDesc, ID3D11Predicate** ppPredicate)
{
	return m_orig->CreatePredicate(pPredicateDesc, ppPredicate);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateCounter(const D3D11_COUNTER_DESC* pCounterDesc, ID3D11Counter** ppCounter)
{
	return m_orig->CreateCounter(pCounterDesc, ppCounter);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext** ppDeferredContext)
{
	return m_orig->CreateDeferredContext(ContextFlags, ppDeferredContext);
}

HRESULT STDMETHODCALLTYPE D3D11Device::OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void** ppResource)
{
	return m_orig->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckFormatSupport(DXGI_FORMAT Format, UINT* pFormatSupport)
{
	return m_orig->CheckFormatSupport(Format, pFormatSupport);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount, UINT* pNumQualityLevels)
{
	return m_orig->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}

void STDMETHODCALLTYPE D3D11Device::CheckCounterInfo(D3D11_COUNTER_INFO* pCounterInfo)
{
	m_orig->CheckCounterInfo(pCounterInfo);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckCounter(const D3D11_COUNTER_DESC* pDesc, D3D11_COUNTER_TYPE* pType, UINT* pActiveCounters, LPSTR szName, UINT* pNameLength, LPSTR szUnits, UINT* pUnitsLength, LPSTR szDescription, UINT* pDescriptionLength)
{
	return m_orig->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}

HRESULT STDMETHODCALLTYPE D3D11Device::CheckFeatureSupport(D3D11_FEATURE Feature, void* pFeatureSupportData, UINT FeatureSupportDataSize)
{
	return m_orig->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

HRESULT STDMETHODCALLTYPE D3D11Device::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
	return m_orig->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
	return m_orig->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
	return m_orig->SetPrivateDataInterface(guid, pData);
}

D3D_FEATURE_LEVEL STDMETHODCALLTYPE D3D11Device::GetFeatureLevel(void)
{
	return m_orig->GetFeatureLevel();
}

UINT STDMETHODCALLTYPE D3D11Device::GetCreationFlags(void)
{
	return m_orig->GetCreationFlags();
}

HRESULT STDMETHODCALLTYPE D3D11Device::GetDeviceRemovedReason(void)
{
	return m_orig->GetDeviceRemovedReason();
}

void STDMETHODCALLTYPE D3D11Device::GetImmediateContext(ID3D11DeviceContext** ppImmediateContext)
{
	m_orig->GetImmediateContext(ppImmediateContext);
}

HRESULT STDMETHODCALLTYPE D3D11Device::SetExceptionMode(UINT RaiseFlags)
{
	return m_orig->SetExceptionMode(RaiseFlags);
}

UINT STDMETHODCALLTYPE D3D11Device::GetExceptionMode(void)
{
	return m_orig->GetExceptionMode();
}

HRESULT STDMETHODCALLTYPE D3D11Device::GetUnderlyingInterface(REFIID riid, void** ppvObject)
{
	return m_orig.query_to(riid, ppvObject);
}
