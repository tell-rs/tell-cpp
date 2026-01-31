// include/tell/types.hpp
// Core enums and event name constants — spec-only types.

#pragma once

#include <cstdint>

namespace tell {

// Schema type for routing batches (§5.1).
enum class SchemaType : uint8_t {
    Unknown = 0,
    Event   = 1,
    Log     = 2,
};

// Event type for analytics events (§5.2).
enum class EventType : uint8_t {
    Unknown = 0,
    Track   = 1,
    Identify = 2,
    Group   = 3,
    Alias   = 4,
    Enrich  = 5,
    Context = 6,
};

// Log event type (§5.3).
enum class LogEventType : uint8_t {
    Unknown = 0,
    Log     = 1,
    Enrich  = 2,
};

// Log severity levels — RFC 5424 + Trace (§10).
enum class LogLevel : uint8_t {
    Emergency = 0,
    Alert     = 1,
    Critical  = 2,
    Error     = 3,
    Warning   = 4,
    Notice    = 5,
    Info      = 6,
    Debug     = 7,
    Trace     = 8,
};

// Standard event names from the Tell specification (Appendix A).
struct Events {
    // User Lifecycle
    static constexpr const char* USER_SIGNED_UP       = "User Signed Up";
    static constexpr const char* USER_SIGNED_IN       = "User Signed In";
    static constexpr const char* USER_SIGNED_OUT      = "User Signed Out";
    static constexpr const char* USER_INVITED         = "User Invited";
    static constexpr const char* USER_ONBOARDED       = "User Onboarded";
    static constexpr const char* AUTHENTICATION_FAILED = "Authentication Failed";
    static constexpr const char* PASSWORD_RESET       = "Password Reset";
    static constexpr const char* TWO_FACTOR_ENABLED   = "Two Factor Enabled";
    static constexpr const char* TWO_FACTOR_DISABLED  = "Two Factor Disabled";

    // Revenue & Billing
    static constexpr const char* ORDER_COMPLETED          = "Order Completed";
    static constexpr const char* ORDER_REFUNDED           = "Order Refunded";
    static constexpr const char* ORDER_CANCELED           = "Order Canceled";
    static constexpr const char* PAYMENT_FAILED           = "Payment Failed";
    static constexpr const char* PAYMENT_METHOD_ADDED     = "Payment Method Added";
    static constexpr const char* PAYMENT_METHOD_UPDATED   = "Payment Method Updated";
    static constexpr const char* PAYMENT_METHOD_REMOVED   = "Payment Method Removed";

    // Subscription
    static constexpr const char* SUBSCRIPTION_STARTED   = "Subscription Started";
    static constexpr const char* SUBSCRIPTION_RENEWED   = "Subscription Renewed";
    static constexpr const char* SUBSCRIPTION_PAUSED    = "Subscription Paused";
    static constexpr const char* SUBSCRIPTION_RESUMED   = "Subscription Resumed";
    static constexpr const char* SUBSCRIPTION_CHANGED   = "Subscription Changed";
    static constexpr const char* SUBSCRIPTION_CANCELED  = "Subscription Canceled";

    // Trial
    static constexpr const char* TRIAL_STARTED      = "Trial Started";
    static constexpr const char* TRIAL_ENDING_SOON  = "Trial Ending Soon";
    static constexpr const char* TRIAL_ENDED        = "Trial Ended";
    static constexpr const char* TRIAL_CONVERTED    = "Trial Converted";

    // Shopping
    static constexpr const char* CART_VIEWED         = "Cart Viewed";
    static constexpr const char* CART_UPDATED        = "Cart Updated";
    static constexpr const char* CART_ABANDONED      = "Cart Abandoned";
    static constexpr const char* CHECKOUT_STARTED    = "Checkout Started";
    static constexpr const char* CHECKOUT_COMPLETED  = "Checkout Completed";

    // Engagement
    static constexpr const char* PAGE_VIEWED          = "Page Viewed";
    static constexpr const char* FEATURE_USED         = "Feature Used";
    static constexpr const char* SEARCH_PERFORMED     = "Search Performed";
    static constexpr const char* FILE_UPLOADED        = "File Uploaded";
    static constexpr const char* NOTIFICATION_SENT    = "Notification Sent";
    static constexpr const char* NOTIFICATION_CLICKED = "Notification Clicked";

    // Communication
    static constexpr const char* EMAIL_SENT                = "Email Sent";
    static constexpr const char* EMAIL_OPENED              = "Email Opened";
    static constexpr const char* EMAIL_CLICKED             = "Email Clicked";
    static constexpr const char* EMAIL_BOUNCED             = "Email Bounced";
    static constexpr const char* EMAIL_UNSUBSCRIBED        = "Email Unsubscribed";
    static constexpr const char* SUPPORT_TICKET_CREATED    = "Support Ticket Created";
    static constexpr const char* SUPPORT_TICKET_RESOLVED   = "Support Ticket Resolved";
};

} // namespace tell
