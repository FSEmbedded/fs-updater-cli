#pragma once

enum class UPDATER_FIRMWARE_STATE : int{
    UPDATE_SUCCESSFUL = 0,
    UPDATE_PROGRESS_ERROR = 1,
    UPDATE_INTERNAL_ERROR = 2,
    UPDATE_SYSTEM_ERROR = 3,
    ROLLBACK_SUCCESSFUL = 4,
    ROLLBACK_PROGRESS_ERROR = 5,
    ROLLBACK_INTERNAL_ERROR = 6,
    ROLLBACK_SYSTEM_ERROR = 7,
};

enum class UPDATER_APPLICATION_STATE : int{
    UPDATE_SUCCESSFUL = 8,
    UPDATE_PROGRESS_ERROR = 9,
    UPDATE_INTERNAL_ERROR = 10,
    UPDATE_SYSTEM_ERROR = 11,
    ROLLBACK_SUCCESSFUL = 12,
    ROLLBACK_PROGRESS_ERROR = 13,
    ROLLBACK_INTERNAL_ERROR = 14,
    ROLLBACK_SYSTEM_ERROR = 15
};

enum class UPDATER_FIRMWARE_AND_APPLICATION_STATE : int{
    UPDATE_SUCCESSFUL = 16,
    UPDATE_PROGRESS_ERROR = 17,
    UPDATE_INTERNAL_ERROR = 18,
    UPDATE_SYSTEM_ERROR = 19
};

enum class UPDATER_COMMIT_STATE : int{
    SUCCESSFUL = 20,
    UPDATE_NOT_NEEDED = 21,
    UPDATE_NOT_ALLOWED_UBOOT_STATE = 22,
    UPDATE_SYSTEM_ERROR = 23
};

enum class UPDATER_UPDATE_REBOOT_STATE : int{
    FAILED_APP_UPDATE = 24,
    FAILED_FW_UPDATE = 25,
    FW_UPDATE_REBOOT_FAILED = 26,
    INCOMPLETE_FW_UPDATE = 27,
    INCOMPLETE_APP_UPDATE = 28,
    INCOMPLETE_APP_FW_UPDATE = 29,
    FW_REBOOT_PENDING = 30,
    APP_REBOOT_PENDING = 31,
    NO_UPDATE_REBOOT_PENDING = 32,
    ROLLBACK_FW_REBOOT_PENDING = 33,
    ROLLBACK_APP_REBOOT_PENDING = 34,
    INCOMPLETE_FW_ROLLBACK = 35,
    INCOMPLETE_APP_ROLLBACK = 36
};

enum class UPDATER_AUTOMATIC_UPDATE_FIRMWARE_STATE : int{
    UPDATE_SUCCESSFULL = 37,
    UPDATE_PROGRESS_ERROR = 38,
    UPDATE_INTERNAL_ERROR = 39,
    UPDATE_SYSTEM_ERROR = 40
};

enum class UPDATER_AUTOMATIC_UPDATE_APPLICATION_STATE : int{
    UPDATE_SUCCESSFULL = 41,
    UPDATE_PROGRESS_ERROR = 42,
    UPDATE_INTERNAL_ERROR = 43,
    UPDATE_SYSTEM_ERROR = 44
};

enum class UPDATER_AUTOMATIC_UPDATE_FIRMWARE_AND_APPLICATION_STATE : int{
    UPDATE_SUCCESSFULL = 45,
    UPDATE_PROGRESS_ERROR = 46,
    UPDATE_INTERNAL_ERROR = 47,
    UPDATE_SYSTEM_ERROR = 48
};

enum class UPDATER_ALLOW_AUTOMATIC_UPDATE_STATE : int{
    UPDATE_SUCCESSFULL = 49,
    FORMER_APPLICATION_UPDATE_FAILED = 50,
    FORMER_FIRMWARE_UPDATE_FAILED = 51,
    FORMER_FIRMWARE_REBOOT_FAILED = 52
};

enum class UPDATER_IS_UPDATE_AVAILABLE_STATE : int{
    NO_UPDATE_AVAILABLE = 53,
    FIRMWARE_UPDATE_AVAILABLE = 54,
    APPLICATION_UPDATE_AVAILABLE = 55,
    FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE = 56
};

enum class UPDATER_DOWNLOAD_UPDATE_STATE : int{
    NO_DOWNLOAD_QUEUED = 57,
    UPDATE_DOWNLOAD_STARTED = 58,
    UPDATE_DOWNLOAD_STARTED_BEFORE = 59
};

enum class UPDATER_DOWNLOAD_PROGRESS_STATE : int{
    NO_DOWNLOAD_STARTED = 60,
    UPDATE_DOWNLOAD_IN_PROGRESS = 61,
    UPDATE_DOWNLOAD_FINISHED = 62
};

enum class UPDATER_INSTALL_UPDATE_STATE : int{
    NO_INSTALLATION_QUEUED = 63,
    UPDATE_INSTALLATION_IN_PROGRESS = 64,
    UPDATE_INSTALLATION_FINISHED = 65,
    UPDATE_INSTALLATION_FAILED = 66
};

enum class UPDATER_APPLY_UPDATE_STATE : int{
    APPLY_SUCCESSFULL = 67,
    APPLY_FAILED = 68
};

enum class UPDATER_SETGET_UPDATE_STATE : int{
    GETSET_STATE_SUCCESSFULL = 69,
    PASSING_PARAM_UPDATE_STATE_WRONG = 70,
    UPDATE_STATE_BAD = 71
};
