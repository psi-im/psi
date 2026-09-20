// SPDX-License-Identifier: GPL-2.0-or-later
#include "avcallaudiodirection.h"
#include "avcallpolicy.h"
#include <iris/jingle-session.h>

namespace RTP    = XMPP::Jingle::RTP;
namespace Jingle = XMPP::Jingle;

AvCallAudioDirection::AvCallAudioDirection(QObject *parent, int deadlineMs) : QObject(parent), deadlineMs_(deadlineMs)
{
}
AvCallAudioDirection::~AvCallAudioDirection() { clear(); }

void AvCallAudioDirection::clear()
{
    if (controller_) {
        disconnect(controller_, nullptr, this, nullptr);
        // Tearing down the policy owner explicitly revokes consent before dropping
        // its device constraint. Merely deleting an operation would not do this.
        if (content_)
            controller_->setLocalSending(content_, false);
    }
    operation_.reset();
    unavailable_.reset();
    content_.clear();
    controller_.clear();
}

void AvCallAudioDirection::bind(RTP::Application *content, bool wanted, bool available)
{
    clear();
    content_          = content;
    captureAvailable_ = available;
    if (content_) {
        controller_ = content_->pad().staticCast<RTP::Pad>()->directionController();
        connect(controller_, &RTP::DirectionController::policyChanged, this, &AvCallAudioDirection::changed);
        if (!available)
            unavailable_ = controller_->suspendLocalSending(content_, QStringLiteral("audio input unavailable"));
        observe(wanted);
    }
    emit changed(); // callbacks may delete the entire call
}

void AvCallAudioDirection::observe(bool desired)
{
    operation_ = controller_->requestLocalSending({ { content_, desired } }, deadlineMs_);
    connect(operation_.get(), &RTP::DirectionOperation::finished, this, &AvCallAudioDirection::changed);
}

void AvCallAudioDirection::setLocalSending(bool enabled)
{
    if (!controller_ || !content_)
        return;
    observe(enabled);
    emit changed();
}

void AvCallAudioDirection::setCaptureAvailable(bool available)
{
    if (available == captureAvailable_)
        return; // repeated capability notifications must not reset the retry budget
    captureAvailable_ = available;
    if (controller_ && content_) {
        const auto policy = controller_->policy(content_);
        if (!available)
            unavailable_ = controller_->suspendLocalSending(content_, QStringLiteral("audio input unavailable"));
        else
            unavailable_.reset();
        if (policy)
            observe(policy->desiredSending); // a new device event, not a new consent decision
    }
    emit changed();
}

bool AvCallAudioDirection::allowsCapture(const RTP::Application *content) const
{
    if (!content || content != content_ || !controller_ || !captureAvailable_
        || content->state() >= Jingle::State::Finishing)
        return false;
    const auto session = content->pad()->session();
    return session && session->state() < Jingle::State::Finishing && controller_->allowsLocalSending(content)
        && AvCallPolicy::allowsSender(content->senders(), session->role());
}
