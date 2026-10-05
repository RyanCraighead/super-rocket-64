float projVel = wheel.m_raycastInfo.m_contactNormalWS.dot(wheel.m_velAtContactPoint);
if (object && object->getUserIndex2() == 0x534d3634 && object->isKinematicObject()) {
	if (btRigidBody* support = btRigidBody::upcast(object)) {
		btVector3 supportRelpos = wheel.m_raycastInfo.m_contactPointWS - support->getCenterOfMassPosition();
		btVector3 supportVelocity = support->getVelocityInLocalPoint(supportRelpos);
		projVel = wheel.m_raycastInfo.m_contactNormalWS.dot(wheel.m_velAtContactPoint - supportVelocity);
	}
}
