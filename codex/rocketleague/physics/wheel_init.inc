// Host build adaptation for the pinned btVehicleRL::addWheel implementation.
// Its temporary wheel otherwise copies uninitialized ray/contact fields into
// the array and exposes an indeterminate suspension length before tick one.
// Initialize only fields not assigned by the pinned constructors. Simulation
// updates continue to use the original backend code and constants.
btWheelInfoRL initialized(ci);
initialized.m_raycastInfo.m_contactNormalWS.setZero();
initialized.m_raycastInfo.m_contactPointWS.setZero();
initialized.m_raycastInfo.m_suspensionLength = suspensionRestLength;
initialized.m_raycastInfo.m_hardPointWS.setZero();
initialized.m_raycastInfo.m_wheelDirectionWS.setZero();
initialized.m_raycastInfo.m_wheelAxleWS.setZero();
initialized.m_raycastInfo.m_isInContact = false;
initialized.m_raycastInfo.m_groundObject = nullptr;
initialized.m_worldTransform.setIdentity();
initialized.m_clientInfo = nullptr;
initialized.m_clippedInvContactDotSuspension = 1;
initialized.m_suspensionRelativeVelocity = 0;
initialized.m_wheelsSuspensionForce = 0;
initialized.m_skidInfo = 1;
initialized.m_velAtContactPoint.setZero();
initialized.m_impulse.setZero();
initialized.m_suspensionForceScale = 0;
m_wheelInfo.push_back(initialized);
