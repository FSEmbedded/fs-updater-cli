/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2024-2026 F&S Elektronik Systeme GmbH
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

enum class UPDATER_FIRMWARE_STATE : int{
    UPDATE_SUCCESSFUL = 0,
    UPDATE_PROGRESS_ERROR = 1,
    UPDATE_INTERNAL_ERROR = 2,
    UPDATE_SYSTEM_ERROR = 3,
};

enum class UPDATER_APPLICATION_STATE : int{
    UPDATE_SUCCESSFUL = 4,
    UPDATE_PROGRESS_ERROR = 5,
    UPDATE_INTERNAL_ERROR = 6,
    UPDATE_SYSTEM_ERROR = 7
};

enum class UPDATER_FIRMWARE_AND_APPLICATION_STATE : int{
    UPDATE_SUCCESSFUL = 8,
    UPDATE_PROGRESS_ERROR = 9,
    UPDATE_INTERNAL_ERROR = 10,
    UPDATE_SYSTEM_ERROR = 11
};

enum class UPDATER_UPDATE_ROLLBACK_STATE : int{
    UPDATE_ROLLBACK_SUCCESSFUL = 12,
    UPDATE_ROLLBACK_PROGRESS_ERROR = 13,
    UPDATE_ROLLBACK_INTERNAL_ERROR = 14,
    UPDATE_ROLLBACK_SYSTEM_ERROR = 15
};

enum class UPDATER_COMMIT_STATE : int{
    UPDATE_COMMIT_SUCCESSFUL = 16,
    UPDATE_NOT_NEEDED = 17,
    UPDATE_NOT_ALLOWED_UBOOT_STATE = 18,
    UPDATE_SYSTEM_ERROR = 19
};

enum class UPDATER_UPDATE_REBOOT_STATE : int{
    FAILED_APP_UPDATE = 20,
    FAILED_FW_UPDATE = 21,
    FW_UPDATE_REBOOT_FAILED = 22,
    INCOMPLETE_FW_UPDATE = 23,
    INCOMPLETE_APP_UPDATE = 24,
    INCOMPLETE_APP_FW_UPDATE = 25,
    UPDATE_REBOOT_PENDING = 26,
    NO_UPDATE_REBOOT_PENDING = 27,
    ROLLBACK_FW_REBOOT_PENDING = 28,
    ROLLBACK_APP_REBOOT_PENDING = 29,
    ROLLBACK_APP_FW_REBOOT_PENDING = 30,
    INCOMPLETE_FW_ROLLBACK = 31,
    INCOMPLETE_APP_ROLLBACK = 32,
    INCOMPLETE_APP_FW_ROLLBACK = 33
};

enum class UPDATER_IS_UPDATE_AVAILABLE_STATE : int{
    NO_UPDATE_AVAILABLE = 34,
    FIRMWARE_UPDATE_AVAILABLE = 35,
    APPLICATION_UPDATE_AVAILABLE = 36,
    FIRMWARE_AND_APPLICATION_UPDATE_AVAILABLE = 37
};

enum class UPDATER_DOWNLOAD_UPDATE_STATE : int{
    NO_DOWNLOAD_QUEUED = 38,
    UPDATE_DOWNLOAD_STARTED = 39,
    UPDATE_DOWNLOAD_STARTED_BEFORE = 40,
    UPDATE_DOWNLOAD_FAILED = 41
};

enum class UPDATER_DOWNLOAD_PROGRESS_STATE : int{
    NO_DOWNLOAD_STARTED = 42,
    UPDATE_DOWNLOAD_WAITING_TO_START = 43,
    UPDATE_DOWNLOAD_IN_PROGRESS = 44,
    UPDATE_DOWNLOAD_FINISHED = 45
};

enum class UPDATER_INSTALL_UPDATE_STATE : int{
    NO_INSTALLATION_QUEUED = 46,
    UPDATE_INSTALLATION_IN_PROGRESS = 47,
    UPDATE_INSTALLATION_FINISHED = 48,
    UPDATE_INSTALLATION_FAILED = 49
};

enum class UPDATER_APPLY_UPDATE_STATE : int{
    APPLY_SUCCESSFUL = 50,
    APPLY_FAILED = 51
};

enum class UPDATER_SETGET_UPDATE_STATE : int{
    GETSET_STATE_SUCCESSFUL = 52,
    PASSING_PARAM_UPDATE_STATE_WRONG = 53,
    UPDATE_STATE_BAD = 54
};

enum class UPDATER_CLI_VALIDATION : int{
    INVALID_UPDATE_TYPE       = 60,  // retired placeholder (was --update_type)
    UPDATE_FILE_NOT_FOUND     = 61,
    MISSING_ENV_UPDATE_STICK  = 62,  // retired placeholder (was --automatic)
    MISSING_ENV_UPDATE_FILE   = 63,  // retired placeholder (was --automatic)
    UPDATE_TYPE_WITHOUT_FILE  = 64,  // retired placeholder (was --update_type)
    INCOMPATIBLE_ARG_COMBO    = 65,
    INSTALL_BUSY              = 66,  // another install/download already in flight
    PERMISSION_DENIED         = 67   // polkit / bus-policy rejected the call
};

enum class UPDATER_SYSTEM : int{
    REBOOT_FAILED             = 70
};

enum class UPDATER_FATAL : int{
    UNHANDLED_EXCEPTION       = 124
};
