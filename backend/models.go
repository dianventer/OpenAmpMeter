package main

import "gorm.io/gorm"

type Sensor struct {
	gorm.Model
	Address   string `gorm:"unique"`
	AddedDate string `gorm:"autoCreateTime"`
	Active    bool   `gorm:"default:true"`
}

type SensorData struct {
	gorm.Model
	SensorID  uint   // This should be the foreign key
	Sensor    Sensor // Foreign key relationship
	Data      string `gorm:"type:jsonb"`
	AddedDate string `gorm:"autoCreateTime"`
}

type Units struct {
	gorm.Model
	Name      string `gorm:"unique"`
	AddedDate string `gorm:"autoCreateTime"`
}

type SensorUnit struct {
	gorm.Model
	SensorID  uint   // This should be the foreign key
	Sensor    Sensor // Foreign key relationship
	UnitID    uint   // This should be the foreign key
	Unit      Units  // Foreign key relationship
	AddedDate string `gorm:"autoCreateTime"`
}

type User struct {
	gorm.Model
	Username  string `gorm:"unique"`
	Password  string
	AddedDate string `gorm:"autoCreateTime"`
}

type UserSensor struct {
	gorm.Model
	UserID    uint   // This should be the foreign key
	User      User   // Foreign key relationship
	SensorID  uint   // This should be the foreign key
	Sensor    Sensor // Foreign key relationship
	AddedDate string `gorm:"autoCreateTime"`
}

type SensorRelation struct {
	gorm.Model
	SensorID  uint   // This should be the foreign key
	Sensor    Sensor // Foreign key relationship
	RelatedID uint   // This should be the foreign key
	Related   Sensor // Foreign key relationship
	AddedDate string `gorm:"autoCreateTime"`
}

type SensorRules struct {
	gorm.Model
	SensorID  uint   // This should be the foreign key
	Sensor    Sensor // Foreign key relationship
	Rule      string `gorm:"type:jsonb"`
	AddedDate string `gorm:"autoCreateTime"`
}

type SensorEvents struct {
	gorm.Model
	SensorID  uint   // This should be the foreign key
	Sensor    Sensor // Foreign key relationship
	Event     string `gorm:"type:jsonb"`
	AddedDate string `gorm:"autoCreateTime"`
}
