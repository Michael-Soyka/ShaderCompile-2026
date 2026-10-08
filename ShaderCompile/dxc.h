//============ Copyright 2026 Moon-6 Team, All rights reserved. ===============//
//
// Purpose: DXC backend, HLSL SM 6.x (cs, vs, ps) to SPIR-V
//
//=============================================================================//

#pragma once

#include "cmdsink.h"

namespace CfgProcessor
{
	struct ComboBuildCommand;
}

namespace Dxc
{
	// Same contract as Compiler::ExecuteCommand, for cs/vs/ps 6.x models
	void ExecuteCommand( const CfgProcessor::ComboBuildCommand& command, CmdSink::IResponse*& pResponse, unsigned int flags );
}
