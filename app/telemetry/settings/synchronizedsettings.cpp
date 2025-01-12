#include "synchronizedsettings.h"

#include "../models/aohdsystem.h"
#include "mavlinksettingsmodel.h"

#include "../../util/WorkaroundMessageBox.h"
#include <sstream>


SynchronizedSettings::SynchronizedSettings(QObject *parent)
    : QObject{parent}
{

}

SynchronizedSettings& SynchronizedSettings::instance()
{
    static SynchronizedSettings tmp;
    return tmp;
}

int SynchronizedSettings::get_param_int_air_and_ground_value(QString param_id)
{
    qDebug()<<"get_param_air_and_ground_value "<<param_id;

    const auto value_ground_opt=MavlinkSettingsModel::instanceGround().try_get_param_int_impl(param_id);
    if(!value_ground_opt.has_value()){
        workaround::makePopupMessage("Cannot fetch param from ground");
        return -1;
    }
    const auto value_ground=value_ground_opt.value();
    // Now that we have the value from the ground, fetch the value from the air
    const auto value_air_opt=MavlinkSettingsModel::instanceAir().try_get_param_int_impl(param_id);
    if(!value_air_opt.has_value()){
        workaround::makePopupMessage("Cannot fetch param from air");
        return value_ground;
    }
    const auto value_air=value_air_opt.value();
    if(value_air!=value_ground){
         workaround::makePopupMessage("Air and ground are out of sync - this should never happen. Please report");
         return value_ground;
    }
    return value_ground;
}


void SynchronizedSettings::change_param_air_and_ground(QString param_id,int value)
{
    qDebug()<<"SynchronizedSettings::change_param_air_and_ground: "<<param_id<<":"<<value;
    // sanity checking
    const bool air_and_ground_alive=AOHDSystem::instanceAir().is_alive() && AOHDSystem::instanceGround().is_alive();
    if(!air_and_ground_alive){
        workaround::makePopupMessage("Precondition: Air and Ground running and alive not given. Change not possible.");
        return;
    }
    const MavlinkSettingsModel::ExtraRetransmitParams extra_retransmit_params{std::chrono::milliseconds(100),10};
    // First change it on the air and wait for ack - if failed, return. MAVSDK does 3 retransmission(s) until acked so it is really unlikely that
    // we set the value and all 3 ack's are lost (which would be the generals problem and then the frequenies are out of sync).
    const bool air_success=MavlinkSettingsModel::instanceAir().try_set_param_int_impl(param_id,value,extra_retransmit_params);
    if(!air_success){
        std::stringstream ss;
        ss<<"Air rejected "<<param_id.toStdString()<<":"<<value<<" nothing changed";
         workaround::makePopupMessage(ss.str().c_str());
        return;
    }
    // we have changed the air freq, now change the ground
    const bool ground_success=MavlinkSettingsModel::instanceGround().try_set_param_int_impl(param_id,value);
    if(!ground_success){
        std::stringstream ss;
        ss<<"Air changed but ground rejected - unfortunately you have to manually fix "<<param_id.toStdString();
        workaround::makePopupMessage(ss.str().c_str());
        return;
    }
    std::stringstream ss;
    ss<<"Successfully changed "<<param_id.toStdString()<<" to "<<value<<" ,might take up to 3 seconds until applied";
    workaround:: makePopupMessage(ss.str().c_str());
}

