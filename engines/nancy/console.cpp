/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "engines/nancy/console.h"
#include "engines/nancy/nancy.h"
#include "engines/nancy/resource.h"
#include "engines/nancy/video.h"
#include "engines/nancy/sound.h"
#include "engines/nancy/iff.h"
#include "engines/nancy/state/scene.h"

#include "common/system.h"
#include "common/events.h"

#include "graphics/surface.h"

#include "audio/audiostream.h"

namespace Nancy {

NancyConsole::NancyConsole(NancyEngine *vm) : GUI::Debugger(), _vm(vm) {
	registerCmd("load_cal", WRAP_METHOD(NancyConsole, Cmd_loadCal));
	registerCmd("cif_hexdump", WRAP_METHOD(NancyConsole, Cmd_cifHexDump));
	registerCmd("cif_export", WRAP_METHOD(NancyConsole, Cmd_cifExport));
	registerCmd("cif_list", WRAP_METHOD(NancyConsole, Cmd_cifList));
	registerCmd("cif_info", WRAP_METHOD(NancyConsole, Cmd_cifInfo));
	registerCmd("chunk_hexdump", WRAP_METHOD(NancyConsole, Cmd_chunkHexDump));
	registerCmd("chunk_list", WRAP_METHOD(NancyConsole, Cmd_chunkList));
	registerCmd("show_image", WRAP_METHOD(NancyConsole, Cmd_showImage));
	registerCmd("play_video", WRAP_METHOD(NancyConsole, Cmd_playVideo));
	registerCmd("play_audio", WRAP_METHOD(NancyConsole, Cmd_playAudio));
	registerCmd("load_scene", WRAP_METHOD(NancyConsole, Cmd_loadScene));
	registerCmd("scene_id", WRAP_METHOD(NancyConsole, Cmd_sceneID));
}

NancyConsole::~NancyConsole() {
}

void NancyConsole::postEnter() {
	GUI::Debugger::postEnter();
	if (!_videoFile.empty()) {
		Video::VideoDecoder *dec = new AVFDecoder;

		if (dec->loadFile(_videoFile)) {
			dec->start();
			_vm->_system->fillScreen(0);
			Common::EventManager *ev = g_system->getEventManager();
			while (!_vm->shouldQuit() && !dec->endOfVideo()) {
				Common::Event event;
				if (ev->pollEvent(event)) {
					if (event.type == Common::EVENT_KEYDOWN || event.type == Common::EVENT_LBUTTONDOWN)
						break;
				}

				if (dec->needsUpdate()) {
					const Graphics::Surface *frame = dec->decodeNextFrame();
					if (frame) {
						_vm->_system->copyRectToScreen(frame->getPixels(), frame->pitch, 0, 0, frame->w, frame->h);
						_vm->_system->updateScreen();
					}
				}
				_vm->_system->delayMillis(10);
			}
		} else
			debugPrintf("Failed to load '%s'\n", _videoFile.c_str());

		_videoFile.clear();
		delete dec;
	}
}

bool NancyConsole::Cmd_cifHexDump(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("Dumps the specified resource to standard output\n");
		debugPrintf("Usage: %s name [cal]\n", argv[0]);
		return true;
	}

	uint size;
	byte *buf = _vm->_res->loadCif((argc == 2 ? "ciftree" : argv[2]), argv[1], size);
	if (!buf) {
		debugPrintf("Failed to load resource '%s'\n", argv[1]);
		return true;
	}

	Common::hexdump(buf, size);
	delete[] buf;
	return true;
}

bool NancyConsole::Cmd_cifExport(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("Exports the specified resource to .cif file\n");
		debugPrintf("Usage: %s name [cal]\n", argv[0]);
		return true;
	}

	if (!_vm->_res->exportCif((argc == 2 ? "ciftree" : argv[2]), argv[1]))
		debugPrintf("Failed to export '%s'\n", argv[1]);

	return true;
}

bool NancyConsole::Cmd_cifList(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("List resources of a certain type\n");
		debugPrintf("Types - 0: all, 2: image, 3: script\n");
		debugPrintf("Usage: %s type [cal]\n", argv[0]);
		return true;
	}

	Common::Array<Common::String> list;
	_vm->_res->list((argc == 2 ? "ciftree" : argv[2]), list, atoi(argv[1]));
	for (uint i = 0; i < list.size(); i++) {
		debugPrintf("%-38s", list[i].c_str());
		if ((i % 2) == 1 && i + 1 != list.size())
			debugPrintf("\n");
	}

	debugPrintf("\n");

	return true;
}

bool NancyConsole::Cmd_cifInfo(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("Prints information about a resource\n");
		debugPrintf("Usage: %s name [cal]\n", argv[0]);
		return true;
	}

	debugPrintf("%s", _vm->_res->getCifDescription((argc == 2 ? "ciftree" : argv[2]), argv[1]).c_str());
	return true;
}

bool NancyConsole::Cmd_chunkHexDump(int argc, const char **argv) {
	if (argc < 3 || argc > 4) {
		debugPrintf("Hexdumps an IFF chunk\n");
		debugPrintf("Usage: %s iffname chunkname [index]\n", argv[0]);
		return true;
	}

	IFF iff(_vm, argv[1]);
	if (!iff.load()) {
		debugPrintf("Failed to load IFF '%s'\n", argv[1]);
		return true;
	}

	const byte *buf;
	uint size;

	char idStr[4] = { ' ', ' ', ' ', ' ' };
	uint len = strlen(argv[2]);
	memcpy(idStr, argv[2], (len <= 4 ? len : 4));
	uint32 id = READ_BE_UINT32(idStr);
	uint index = 0;

	if (argc == 4)
		index = atoi(argv[3]);

	buf = iff.getChunk(id, size, index);
	if (!buf) {
		debugPrintf("Failed to find chunk '%s' (index %d) in IFF '%s'\n", argv[2], index, argv[1]);
		return true;
	}

	Common::hexdump(buf, size);
	return true;
}

bool NancyConsole::Cmd_chunkList(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("List chunks inside an IFF\n");
		debugPrintf("Usage: %s iffname\n", argv[0]);
		return true;
	}

	IFF iff(_vm, argv[1]);
	if (!iff.load()) {
		debugPrintf("Failed to load IFF '%s'\n", argv[1]);
		return true;
	}

	Common::Array<Common::String> list;
	iff.list(list);
	for (uint i = 0; i < list.size(); i++) {
		debugPrintf("%-6s", list[i].c_str());
		if ((i % 13) == 12 && i + 1 != list.size())
			debugPrintf("\n");
	}

	debugPrintf("\n");

	return true;
}

bool NancyConsole::Cmd_showImage(int argc, const char **argv) {
	if (argc < 2 || argc > 3) {
		debugPrintf("Draws an image on the screen\n");
		debugPrintf("Usage: %s name [cal]\n", argv[0]);
		return true;
	}

	Graphics::Surface surf;
	if (_vm->_res->loadImage((argc == 2 ? "ciftree" : argv[2]), argv[1], surf)) {
		_vm->_system->fillScreen(0);
		_vm->_system->copyRectToScreen(surf.getPixels(), surf.pitch, 0, 0, surf.w > 640 ? 640 : surf.w, surf.h > 480 ? 480 : surf.h);
		surf.free();
		_vm->_gameFlow.minGameState = NancyEngine::kIdle;
		return cmdExit(0, 0);
	} else {
		debugPrintf("Failed to load image\n");
		return true;
	}
}

bool NancyConsole::Cmd_loadCal(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Loads a .cal file\n");
		debugPrintf("Usage: %s <name>\n", argv[0]);
		return true;
	}

	if (!_vm->_res->loadCifTree(argv[1], "cal"))
		debugPrintf("Failed to load '%s.cal'\n", argv[1]);
	return true;
}

bool NancyConsole::Cmd_playVideo(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Plays a video\n");
		debugPrintf("Usage: %s <name>\n", argv[0]);
		return true;
	}

	_videoFile = argv[1];
	_videoFile += ".avf";
	return cmdExit(0, 0);
}

bool NancyConsole::Cmd_playAudio(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Plays an audio file\n");
		debugPrintf("Usage: %s <name>\n", argv[0]);
		return true;
	}

	Common::File *f = new Common::File;
	if (!f->open(Common::String(argv[1]) + ".his")) {
		debugPrintf("Failed to open '%s.his'\n", argv[1]);
		return true;
	}

	Audio::AudioStream *stream = SoundManager::makeHISStream(f, DisposeAfterUse::YES);

	if (!stream) {
		debugPrintf("Failed to load '%s.his'\n", argv[1]);
		delete f;
		return true;
	}
	Audio::SoundHandle handle;
	_vm->_system->getMixer()->playStream(Audio::Mixer::kPlainSoundType, &handle, stream);
	return true;
}

bool NancyConsole::Cmd_loadScene(int argc, const char **argv) {
	if (argc != 2) {
		debugPrintf("Loads a scene\n");
		debugPrintf("Usage: %s sceneID\n", argv[0]);
		return true;
	}
	
	if (_vm->getPreviousGameState() != NancyEngine::GameState::kScene) {
		debugPrintf("Not in the kScene state\n");
		return true;
	}

	Common::String sceneName = Common::String::format("S%s", argv[1]);
    IFF iff(_vm, sceneName);
	if (!iff.load()) {
		debugPrintf("Invalid scene S%s\n", argv[1]);
		return true;
	}

	_vm->scene->changeScene((uint16)atoi(argv[1]), 0, 0, false);
	_vm->scene->_state = State::Scene::kLoadNew;
	return cmdExit(0, 0);
}

bool NancyConsole::Cmd_sceneID(int argc, const char **argv) {
	if (_vm->getPreviousGameState() != NancyEngine::GameState::kScene) {
		debugPrintf("Not in the kScene state\n");
		return true;
	}

	debugPrintf("Scene: %u, Frame: %i \n", _vm->scene->getSceneInfo().sceneID, _vm->scene->getSceneInfo().frameID);
	return true;
}

} // End of namespace Nancy