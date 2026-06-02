// CodeVisionStencil.h
//
// SIB-25 — Reserved CustomDepth stencil range for the Code Vision reveal
// system (Ch1). Reserve 250–254 for Code Vision so nothing else collides
// (CV2). Document this range in the repo README's "reserved ranges" section.
//
// Requires project setting: Project Settings -> Rendering -> Postprocessing ->
// Custom Depth-Stencil Pass = "Enabled with Stencil". Set once, by hand; a
// fresh clone needs it.

#pragma once

#define CODEVISION_STENCIL 250
