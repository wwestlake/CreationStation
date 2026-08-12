#pragma once

// The generic track/clip type shape (kinds, automation types, storage-token/display-name
// conversions, evaluateAutomationSegment, canTrackContainClip) now lives in shared/Timeline so
// other suite apps (e.g. Creation Movie) can build their own timeline on the same core. This
// header stays as a thin cs:: alias layer so the ~40+ existing `cs::TrackKind` etc. call sites
// across CreationStation don't need to change.
#include <creation/timeline/TimelineTypes.h>

namespace cs
{
using TrackKind = creation::timeline::TrackKind;
using ClipKind = creation::timeline::ClipKind;
using TrackChannelMode = creation::timeline::TrackChannelMode;
using AutomationCurveShape = creation::timeline::AutomationCurveShape;
using AutomationPoint = creation::timeline::AutomationPoint;
using AutomationTargetKind = creation::timeline::AutomationTargetKind;
using AutomationValueMode = creation::timeline::AutomationValueMode;
using AutomationTarget = creation::timeline::AutomationTarget;
using AutomationRecordMode = creation::timeline::AutomationRecordMode;
using TimelineTrack = creation::timeline::TimelineTrack;
using TimelineMarker = creation::timeline::TimelineMarker;

using creation::timeline::toStorageToken;
using creation::timeline::trackKindFromStorageToken;
using creation::timeline::clipKindFromStorageToken;
using creation::timeline::trackChannelModeFromStorageToken;
using creation::timeline::toDisplayName;
using creation::timeline::defaultClipKindForTrack;
using creation::timeline::automationCurveShapeFromStorageToken;
using creation::timeline::automationTargetKindFromStorageToken;
using creation::timeline::automationValueModeFromStorageToken;
using creation::timeline::quantizeAutomationValueForTarget;
using creation::timeline::automationRecordModeFromStorageToken;
using creation::timeline::evaluateAutomationSegment;
using creation::timeline::canTrackContainClip;
}
