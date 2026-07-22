#include	<iostream>
#include	<fstream>
#include	"CTexture.h"
#include	"stb_image.h"
#include	"renderer.h"

// u8�Ή�
bool CTexture::Load(const std::u8string& filename)
{
	std::filesystem::path filepath = filename;
	m_texname.assign(filename.begin(), filename.end());  // UTF-8�Ƃ��ĕێ�

	std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
	if (!ifs) {
		std::cerr << "Failed to open file: " << filepath << std::endl;
		return false;
	}

	std::streamsize size = ifs.tellg();
	ifs.seekg(0, std::ios::beg);

	std::vector<unsigned char> buffer(size);
	if (!ifs.read(reinterpret_cast<char*>(buffer.data()), size)) {
		std::cerr << "Failed to read file: " << filepath << std::endl;
		return false;
	}

	unsigned char* pixels = stbi_load_from_memory(
		buffer.data(), static_cast<int>(buffer.size()),
		&m_width, &m_height, &m_bpp, STBI_rgb_alpha);

	if (!pixels) {
		std::cerr << "Failed to decode image: " << filepath << std::endl;
		return false;
	}

	ComPtr<ID3D11Texture2D> pTexture;
	D3D11_TEXTURE2D_DESC desc{};
	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = m_width * 4;

	HRESULT hr = Renderer::GetDevice()->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	hr = Renderer::GetDevice()->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	stbi_image_free(pixels);

	return SUCCEEDED(hr);
}

// �e�N�X�`�������[�h
bool CTexture::Load(const std::string& filename)
{
	bool sts = true;


	// �摜�ǂݍ���
//	pixels = stbi_load(filename.c_str(), &m_width, &m_height, &m_bpp, 4);
//	if (pixels == nullptr) {
//		std::cout << filename.c_str() << " Load error " << std::endl;

		// u8����������Ɂ@2025/7/12 by suzuki.tomoki
		std::u8string u8s(filename.begin(),filename.end());
		sts =Load(u8s);
		if (sts == false) {
			return false;
		}
		return true;
//	}

/*
// �e�N�X�`��2D���\�[�X����
	ComPtr<ID3D11Texture2D> pTexture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// RGBA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = desc.Width * 4;			// RGBA = 4 bytes per pixel
	subResource.SysMemSlicePitch = 0;

	ID3D11Device* device = Renderer::GetDevice();

	HRESULT hr = device->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// SRV����
	hr = device->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// �s�N�Z���C���[�W���
	stbi_image_free(pixels);

	return true;


*/
}

// �e�N�X�`�������������烍�[�h
bool CTexture::LoadFromFemory(const unsigned char* Data,int len) {

	bool sts = true;
	unsigned char* pixels;

	// �摜�ǂݍ���
	pixels = stbi_load_from_memory(Data, 
		len, 
		&m_width, 
		&m_height, 
		&m_bpp, 
		STBI_rgb_alpha);

	// �e�N�X�`��2D���\�[�X����
	ComPtr<ID3D11Texture2D> pTexture;

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = m_width;
	desc.Height = m_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;			// RGBA
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource{};
	subResource.pSysMem = pixels;
	subResource.SysMemPitch = desc.Width * 4;			// RGBA = 4 bytes per pixel
	subResource.SysMemSlicePitch = 0;

	ID3D11Device* device = Renderer::GetDevice();

	HRESULT hr = device->CreateTexture2D(&desc, &subResource, pTexture.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// SRV����
	hr = device->CreateShaderResourceView(pTexture.Get(), nullptr, m_srv.GetAddressOf());
	if (FAILED(hr)) {
		stbi_image_free(pixels);
		return false;
	}

	// �s�N�Z���C���[�W���
	stbi_image_free(pixels);

	return true;
}

// �e�N�X�`����GPU�ɃZ�b�g
void CTexture::SetGPU()
{
	ID3D11DeviceContext* devicecontext = Renderer::GetDeviceContext();
	devicecontext->PSSetShaderResources(0, 1, m_srv.GetAddressOf());
}
