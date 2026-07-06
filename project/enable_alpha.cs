using System;
using System.IO;

class Program {
    static void Main() {
        string path = @"C:\Users\k024g\OneDrive\デスクトップ\2年\2年前期\CG2\CG2\project\2d\SpriteCommon.cpp";
        string content = File.ReadAllText(path);

        string search = @"blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;";
        string replace = @"blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;";

        content = content.Replace(search, replace);

        File.WriteAllText(path, content, new System.Text.UTF8Encoding(true));
        Console.WriteLine("Enabled alpha blending in SpriteCommon.cpp");
    }
}
