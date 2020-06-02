/*
 *	Copyright (C) 2007-2009 Gabest
 *	http://www.gabest.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "stdafx.h"
#include <glslang\SPIRV\GlslangToSpv.h>
#include "GSSpirVShaderCompiler.h"

GSSpirVShaderCompiler::GSSpirVShaderCompiler(const std::string &shader_string)
    : m_shader_string(shader_string)
{
}

GSSpirVShaderCompiler::~GSSpirVShaderCompiler()
{
}

bool GSSpirVShaderCompiler::Preprocess(const MacroDefinitions *macros, shaderc_shader_kind shader_type)
{
    shaderc::CompileOptions options;
    shaderc::PreprocessedSourceCompilationResult result;

	if (macros != nullptr) {
		for (auto &macro : *macros) {
		    options.AddMacroDefinition(macro.first, macro.second);
		}
    }

	result = m_compiler.PreprocessGlsl(
        m_shader_string,
        shader_type,
        "shader_src",
        options);

	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        fprintf(stdout, "Shader preprocessing error!\n%s", result.GetErrorMessage().c_str());
        return false;
    }

	m_shader_string = {result.cbegin(),
                       result.cend()};

	return true;
}

bool GSSpirVShaderCompiler::Compile(shaderc_shader_kind shader_type, shaderc_optimization_level opt_level, bool debug_compile)
{
    shaderc::CompileOptions options;
    shaderc::SpvCompilationResult result;

	if (debug_compile) {
		options.SetGenerateDebugInfo();
    }

	options.SetOptimizationLevel(opt_level);

	result = m_compiler.CompileGlslToSpv(
        m_shader_string,
        shader_type,
        "shader_src",
        options);

	if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
        fprintf(stdout, "Shader compiling error!\n%s", result.GetErrorMessage().c_str());
        return false;
	}

	m_shader_bytecode = {result.cbegin(),
                         result.cend()};

	return true;
}

void GSSpirVShaderCompiler::CleanupCompiler()
{
    m_shader_bytecode.clear();
}

const uint32 *GSSpirVShaderCompiler::GetShaderBytecode()
{
    return m_shader_bytecode.data();
}

size_t GSSpirVShaderCompiler::GetShaderBytecodeLength()
{
    return m_shader_bytecode.size();
}

void GSSpirVShaderCompiler::TestCompiler()
{
    static const char *test_shader = "\
		#version 450 core\n\
		layout(location = 0) out vec4 outColor; \
		void main() {\
			outColor = vec4(1.0, 0.0, 0.0, 1.0);\
		}";

    GSSpirVShaderCompiler compiler(test_shader);
    compiler.Preprocess(nullptr, shaderc_fragment_shader);
    compiler.Compile(shaderc_fragment_shader);
}
