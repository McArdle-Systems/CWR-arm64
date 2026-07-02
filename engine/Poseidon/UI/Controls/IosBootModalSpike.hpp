#pragma once

namespace Poseidon
{

// TEMPORARY (iOS game-data gate, M0 de-risking spike): presents a blocking
// UIAlertController before app.Run(), pumped via a nested NSRunLoop, to prove
// UIKit works and can be synchronously awaited from inside SDL's iOS main()
// call stack -- before SDL has created any window. No-op on non-iOS.
// Superseded by IosGameDataGateScreen once that lands (M8); delete then.
void RunIosBootModalSpike();

} // namespace Poseidon
