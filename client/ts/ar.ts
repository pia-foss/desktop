<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="en_US">
<context>
    <name>NetworkPage</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="198"/>
        <source>NetworkPage --- Choose which applications use the VPN.</source>
        <extracomment>Description for the split tunnel setting.</extracomment>
        <translation>اختر التطبيقات التي تستخدم VPN.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="202"/>
        <source>NetworkPage --- Bypass VPN - App always connects directly to the Internet</source>
        <extracomment>Description for the &quot;Bypass VPN&quot; split tunnel mode that can be applied to a specific app. These apps do not use the VPN connection, they connect directly to the Internet.</extracomment>
        <translation>تجاوز VPN - يتصل التطبيق دائمًا بالإنترنت مباشرةً</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="206"/>
        <source>NetworkPage --- Only VPN - App can only connect when the VPN is connected</source>
        <extracomment>Description for the &quot;Only VPN&quot; split tunnel mode that can be applied to a specific app. These apps are only allowed to connect via the VPN, they are blocked if the VPN is not connected.</extracomment>
        <translation>VPN فقط - لا يمكن للتطبيق الاتصال بالإنترنت سوى عبر VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="212"/>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="307"/>
        <source>NetworkPage --- Split Tunnel</source>
        <translation>نفق مقسّم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="32"/>
        <source>NetworkPage --- Name Servers</source>
        <extracomment>Label for the setting that controls which DNS servers are used to look up domain names and translate them to IP addresses when browsing the internet. This setting is also present in OS network settings, so this string should preferably match whatever localized term the OS uses.</extracomment>
        <translation>خوادم الأسماء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="69"/>
        <source>NetworkPage --- Warning: Using a third party DNS could compromise your privacy.</source>
        <translation>تحذير: قد يؤدي استخدام بروتوكول DNS خاص بجهة خارجية إلى انتهاك خصوصيتك.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="75"/>
        <source>NetworkPage --- Handshake is a decentralized naming protocol.  For more information, visit handshake.org.</source>
        <extracomment>&quot;Handshake&quot; is a brand name and should not be translated.</extracomment>
        <translation>Handshake هو بروتوكول تسمية لامركزي. لمزيد من المعلومات، تفضل بزيارة handshake.org.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="81"/>
        <source>NetworkPage --- PIA DNS</source>
        <translation>PIA DNS</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="84"/>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="136"/>
        <source>NetworkPage --- Use Existing DNS</source>
        <translation>استخدام DNS موجود</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="85"/>
        <source>NetworkPage --- Set Custom DNS...</source>
        <translation>تعيين DNS مخصص...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="97"/>
        <source>NetworkPage --- Proceed</source>
        <translation>متابعة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="98"/>
        <source>NetworkPage --- Cancel</source>
        <translation>إلغاء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="107"/>
        <source>NetworkPage --- Primary DNS</source>
        <translation>بروتوكول DNS الأساسي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="114"/>
        <source>NetworkPage --- Secondary DNS (optional)</source>
        <translation>بروتوكول DNS الثانوي (اختياري)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="122"/>
        <source>NetworkPage --- &lt;b&gt;Warning:&lt;/b&gt; Using non-PIA DNS servers could expose your DNS traffic to third parties and compromise your privacy.</source>
        <translation>&lt;b&gt;تحذير:&lt;/b&gt; قد يؤدي استخدام خوادم DNS غير المزودة عن طريق PIA إلى كشف حركة مرور DNS بأجهزتك لأطراف ثالثة وتهديد خصوصيتك.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="127"/>
        <source>NetworkPage --- Set Custom DNS</source>
        <translation>تعيين DNS مخصص</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="136"/>
        <source>NetworkPage --- Use Custom DNS</source>
        <translation>استخدام DNS مخصص</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="172"/>
        <source>NetworkPage --- Request Port Forwarding</source>
        <extracomment>Label for the setting that controls whether the application tries to forward a port from the public VPN IP to the user&apos;s computer. This feature is not guaranteed to work or be available, therefore we label it as &quot;requesting&quot; port forwarding.</extracomment>
        <translation>طلب إعادة توجيه المنافذ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="179"/>
        <source>NetworkPage --- Forwards a port from the VPN IP to your computer. The port will be selected for you. Not all locations support port forwarding.</source>
        <extracomment>Tooltip for the port forwarding setting. The user can not choose which port to forward; a port will be automatically assigned by our servers. The user should further be made aware that only some of our servers support forwarding. The string contains embedded linebreaks to prevent it from being displayed too wide on the user&apos;s screen - such breaks should be preserved at roughly the same intervals.</extracomment>
        <translation>يعيد توجيه المنفذ من IP VPN إلى جهاز الكمبيوتر. سوف يتم تحديد المنفذ لأجلك. لا تدعم جميع المواقع إعادة توجيه المنافذ.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="184"/>
        <source>NetworkPage --- Allow LAN Traffic</source>
        <translation>السماح بحركة مرور LAN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="185"/>
        <source>NetworkPage --- Always permits traffic between devices on your local network, even when using the VPN killswitch.</source>
        <translation>يسمح دائمًا بحركة المرور بين الأجهزة على شبكتك المحلية، حتى عند استخدام مفتاح إنهاء VPN.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="440"/>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="445"/>
        <source>NetworkPage --- App Exclusions</source>
        <translation>استبعاد التطبيقات</translation>
    </message>
    <message>
    <source>NetworkPage --- Excluded apps bypass the VPN and connect directly to the Internet.</source>
        <translation>تتجاوز التطبيقات المستبعدة خدمة VPN وتتصل بالإنترنت مباشرة.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="238"/>
        <source>NetworkPage --- Approve the split tunnel extension to enable this feature.</source>
        <translation>يرجى الموافقة على ملحق النفق المقسم لتمكين هذه الميزة.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="249"/>
        <source>NetworkPage --- Security Preferences</source>
        <translation>تفضيلات الأمان</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="363"/>
        <source>NetworkPage --- Installing split tunnel filter...</source>
        <translation>جارٍ تثبيت مرشح النفق المقسم...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="372"/>
        <source>NetworkPage --- Enabling this feature will install the split tunnel filter.</source>
        <translation>سيؤدي تمكين هذه الميزة إلى تثبيت مرشح النفق المقسم.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="379"/>
        <source>NetworkPage --- The split tunnel filter is not installed.  Reinstall it on the Help page.</source>
        <translation>لم يتم تثبيت مرشح النفق المقسم. يرجى إعادة تثبيته من صفحة المساعدة.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/NetworkPage.qml" line="486"/>
        <source>NetworkPage --- Apps may need to be restarted for changes to be applied.</source>
        <translation>قد تحتاج إلى إعادة تشغيل التطبيقات لتطبيق التغييرات.</translation>
    </message>
</context>
<context>
    <name>SplitTunnelDefaultRow</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelDefaultRow.qml" line="82"/>
        <source>SplitTunnelDefaultRow --- All Other Apps</source>
        <translation>كل التطبيقات الأخرى</translation>
    </message>
</context>
<context>
    <name>SplitTunnelRowBase</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelRowBase.qml" line="23"/>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelRowBase.qml" line="27"/>
        <source>SplitTunnelRowBase --- Bypass VPN</source>
        <translation>تجاوز VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelRowBase.qml" line="24"/>
        <source>SplitTunnelRowBase --- Only VPN</source>
        <translation>VPN فقط</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelRowBase.qml" line="28"/>
        <source>SplitTunnelRowBase --- Use VPN</source>
        <translation>استخدام VPN</translation>
    </message>
</context>
<context>
    <name>SplitTunnelSettings</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelSettings.qml" line="293"/>
        <source>SplitTunnelSettings --- Behavior</source>
        <extracomment>Screen reader annotation for the column in the split tunnel app list that displays the behavior selected for a specific app.</extracomment>
        <translation>السلوك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelSettings.qml" line="55"/>
        <source>SplitTunnelSettings --- Applications</source>
        <translation>التطبيقات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelSettings.qml" line="278"/>
        <source>SplitTunnelSettings --- App</source>
        <extracomment>Screen reader annotation for the column in the split tunnel app list that displays app names.</extracomment>
        <translation>التطبيق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelSettings.qml" line="286"/>
        <source>SplitTunnelSettings --- Path</source>
        <extracomment>Screen reader annotation for the column in the split tunnel app list that displays app file paths. (These are visually placed below the app names, but they&apos;re annotated as a separate column.)</extracomment>
        <translation>المسار</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelSettings.qml" line="300"/>
        <source>SplitTunnelSettings --- Remove</source>
        <extracomment>Screen reader annotation for the column in the split tunnel app list that removes a selected app.</extracomment>
        <translation>إزالة</translation>
    </message>
</context>
<context>
    <name>AccountModule</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/AccountModule.qml" line="17"/>
        <source>AccountModule --- Subscription tile</source>
        <extracomment>Screen reader annotation for the &quot;Subscription&quot; tile, should reflect the name of the tile</extracomment>
        <translation>لوحة الاشتراك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/AccountModule.qml" line="22"/>
        <source>AccountModule --- SUBSCRIPTION</source>
        <translation>الاشتراك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/AccountModule.qml" line="35"/>
        <source>AccountModule --- Subscription</source>
        <extracomment>Screen reader annotation for the subscription status display in the Subscription tile, usually the same as the tile name (but not all-caps)</extracomment>
        <translation>الاشتراك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/AccountModule.qml" line="39"/>
        <source>AccountModule --- Expired</source>
        <translation>منتهي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/AccountModule.qml" line="42"/>
        <source>AccountModule --- (%1 days left)</source>
        <translation>(متبقي %1 أيام)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/AccountModule.qml" line="58"/>
        <source>AccountModule --- Expired on</source>
        <translation>انتهى في</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/AccountModule.qml" line="58"/>
        <source>AccountModule --- Renews on</source>
        <translation>يتجدد في</translation>
    </message>
</context>
<context>
    <name>AccountPage</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="24"/>
        <source>AccountPage --- Username</source>
        <translation>اسم المستخدم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="44"/>
        <source>AccountPage --- Subscription</source>
        <translation>الاشتراك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="54"/>
        <source>AccountPage --- Expired</source>
        <translation>منتهي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="61"/>
        <source>AccountPage --- (expired on %1)</source>
        <translation>(انتهى في %1)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="63"/>
        <source>AccountPage --- (renews on %1)</source>
        <translation>(يتجدد في %1)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="65"/>
        <source>AccountPage --- (expires on %1)</source>
        <translation>(ينتهي في %1)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="80"/>
        <source>AccountPage --- Renews in %1 days</source>
        <translation>يتجدد خلال %1 أيام</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="80"/>
        <source>AccountPage --- Expires in %1 days</source>
        <translation>ينتهي خلال %1 أيام</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="85"/>
        <source>AccountPage --- Purchase Subscription</source>
        <translation>شراء اشتراك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="85"/>
        <source>AccountPage --- Manage Subscription</source>
        <translation>إدارة الاشتراك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="85"/>
        <source>AccountPage --- Renew Subscription</source>
        <translation>تجديد الاشتراك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="98"/>
        <source>AccountPage --- Manage My Account</source>
        <translation>إدارة حسابي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="103"/>
        <source>AccountPage --- Logout / Switch Account</source>
        <translation>تسجيل الخروج/تبديل الحساب</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/AccountPage.qml" line="116"/>
        <source>AccountPage --- Not logged in</source>
        <translation>لم يتم تسجيل الدخول</translation>
    </message>
</context>
<context>
    <name>BelowFold</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SeparatorModule.qml" line="19"/>
        <source>BelowFold --- DEFAULT DISPLAY</source>
        <translation>الشاشة الافتراضية</translation>
    </message>
</context>
<context>
    <name>BetaAgreementDialog</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/BetaAgreementDialog.qml" line="15"/>
        <source>BetaAgreementDialog --- Agreement</source>
        <translation>الاتفاقية</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/BetaAgreementDialog.qml" line="19"/>
        <source>BetaAgreementDialog --- Accept</source>
        <extracomment>&quot;Accept&quot; button for accepting the Beta agreement, should use the typical terminology for accepting a legal agreement.</extracomment>
        <translation>قبول</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/BetaAgreementDialog.qml" line="22"/>
        <source>BetaAgreementDialog --- Decline</source>
        <extracomment>&quot;Decline&quot; button for declining the Beta agreement, should use the typical terminology for declining a legal agreement.</extracomment>
        <translation>رفض</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/BetaAgreementDialog.qml" line="62"/>
        <source>BetaAgreementDialog --- Agreement text</source>
        <extracomment>Screen reader annotation for the beta license agreement content (a text element containing the license agreement).</extracomment>
        <translation>نص الاتفاقية</translation>
    </message>
</context>
<context>
    <name>ChangelogWindow</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/ChangelogWindow.qml" line="16"/>
        <source>ChangelogWindow --- Changelog</source>
        <translation>سجل التغييرات</translation>
    </message>
</context>
<context>
    <name>Client</name>
    <message>
        <location filename="../../../../../client/res/components/client/Client.qml" line="127"/>
        <source>Client --- %1 - Best</source>
        <extracomment>Text that indicates the best (lowest ping) region is being used for a given country. The %1 placeholder contains the name of the country, e.g &quot;UNITED STATES - BEST&quot;</extracomment>
        <translation>%1 - الأفضل</translation>
    </message>
</context>
<context>
    <name>ClientNotifications</name>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="35"/>
        <source>ClientNotifications -- header-title --- ERROR</source>
        <comment>header-title</comment>
        <extracomment>Header bar title used for all &quot;error&quot; statuses - serious installation problems, etc. This means that there is currently an error condition active now.</extracomment>
        <translation>خطأ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="51"/>
        <source>ClientNotifications --- UPDATE FAILED</source>
        <translation>فشل التحديث</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="52"/>
        <source>ClientNotifications --- Download of version %1 failed.</source>
        <translation>تعذّر تنزيل الإصدار %1.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="71"/>
        <source>ClientNotifications --- The virtual network adapter is not installed.</source>
        <translation>محول الشبكة الظاهرية غير مثبت.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="74"/>
        <source>ClientNotifications --- The TAP adapter for the VPN tunnel is not installed.  You can reinstall it from Settings.</source>
        <extracomment>&quot;TAP&quot; is the type of virtual network adapter used on Windows and is not generally localized.</extracomment>
        <translation>لم يتم تثبيت محول TAP لنفق VPN.  يمكنك إعادة تثبيته من الإعدادات.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="78"/>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="135"/>
        <source>ClientNotifications --- Reinstall</source>
        <translation>إعادة التثبيت</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="111"/>
        <source>ClientNotifications --- Restart to complete installation.</source>
        <translation>إعادة التشغيل لإكمال التثبيت.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="112"/>
        <source>ClientNotifications --- The system must be restarted before you can connect.</source>
        <translation>يجب إعادة تشغيل النظام قبل أن تتمكن من الاتصال.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="123"/>
        <source>ClientNotifications --- The split tunnel filter is not installed.</source>
        <translation>لم يتم تثبيت مرشح النفق المقسم.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="124"/>
        <source>ClientNotifications --- The App Exclusion feature requires the split tunnel filter.  Reinstall it from Settings.</source>
        <translation>تتطلب ميزة استبعاد التطبيقات تثبيت مرشح النفق المقسم. أعد تثبيته من الإعدادات.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="149"/>
        <source>ClientNotifications --- The App Exclusion feature requires the split tunnel filter.  Restart to finish installation.</source>
        <translation>تتطلب ميزة استبعاد التطبيقات تثبيت مرشح النفق المقسم. أعد التشغيل لإكمال التثبيت.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="164"/>
        <source>ClientNotifications --- Connection refused.</source>
        <translation>تم رفض الاتصال.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="170"/>
        <source>ClientNotifications --- The server refused the connection.  Please check your username and password, and verify that your account is not expired.</source>
        <extracomment>This error could be caused by incorrect credentials or an expired account, but it could have other causes too. The message should suggest checking those things without implying that they&apos;re necessarily the cause (to avoid frustrating users who are sure their account is current).</extracomment>
        <translation>رفض الخادم الاتصال.  يرجى التحقق من اسم المستخدم وكلمة المرور، والتحقق من عدم انتهاء صلاحية حسابك.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="179"/>
        <source>ClientNotifications --- Could not configure DNS.</source>
        <translation>تعذر تكوين DNS.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="180"/>
        <source>ClientNotifications --- Enable debug logging and check the daemon log for specific details.</source>
        <translation>مكن تسجيل التصحيح وفحص سجل البرنامج الخفي للحصول على تفاصيل محددة.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="183"/>
        <source>ClientNotifications --- Daemon Log</source>
        <translation>سجل البرنامج الخفي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="185"/>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="217"/>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="276"/>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="331"/>
        <source>ClientNotifications --- Settings</source>
        <translation>الإعدادات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="193"/>
        <source>ClientNotifications --- Failed to run /usr/bin/xdg-open.  Please open the daemon log file from:</source>
        <translation>فشل تشغيل /usr/bin/xdg-open. يرجى فتح ملف سجل البرنامج الخفي من:</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="210"/>
        <source>ClientNotifications --- Can&apos;t connect to Handshake.</source>
        <extracomment>Indicates that we can&apos;t connect to the Handshake name-resolution network. &quot;Handshake&quot; is a brand name and should be left as-is.</extracomment>
        <translation>يتعذّر الاتصال بـ Handshake.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="214"/>
        <source>ClientNotifications --- Can&apos;t set up name resolution with Handshake.  Continue waiting, or try a different Name Server setting.</source>
        <extracomment>Detailed message about failure to connect to the Handshake name- resolution network. &quot;Handshake&quot; is a brand name and should be left as-is.</extracomment>
        <translation>يتعذّر إعداد تحليل الاسم مع Handshake. استمر في الانتظار، أو جرّب إعداد اسم خادم مختلف.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="228"/>
        <source>ClientNotifications --- Running PIA as administrator is not recommended.</source>
        <translation>لا يُنصح بتشغيل PIA كمسؤول.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="229"/>
        <source>ClientNotifications --- Running PIA as administrator can prevent Launch on System Startup from working and may cause other problems.</source>
        <translation>يمكن أن يؤدي تشغيل PIA كمسؤول إلى منع بدء التشغيل عند بدء تشغيل النظام وقد يتسبب في حدوث مشاكل أخرى.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="241"/>
        <source>ClientNotifications --- KILLSWITCH ENABLED</source>
        <translation>تم تمكين مفتاح الإنهاء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="242"/>
        <source>ClientNotifications --- Killswitch is enabled.</source>
        <translation>تم تمكين مفتاح الإنهاء.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="243"/>
        <source>ClientNotifications --- Access to the Internet is blocked because the killswitch feature is enabled in Settings.</source>
        <translation>تم حظر الوصول إلى الإنترنت بسبب تمكين مفتاح الإنهاء في الإعدادات.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="246"/>
        <source>ClientNotifications --- Change</source>
        <translation>تغيير</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="259"/>
        <source>ClientNotifications --- RECONNECTING...</source>
        <translation>جارٍ إعادة الاتصال...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="260"/>
        <source>ClientNotifications --- The connection to the VPN server was lost.</source>
        <translation>تم فقد الاتصال بخادم VPN.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="269"/>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="287"/>
        <source>ClientNotifications --- CONNECTING...</source>
        <translation>جارٍ الاتصال...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="272"/>
        <source>ClientNotifications --- Can&apos;t connect to the proxy.</source>
        <extracomment>Warning message used when the app is currently trying to connect to a proxy, but the proxy can&apos;t be reached.</extracomment>
        <translation>تعذر الاتصال بالبروكسي.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="273"/>
        <source>ClientNotifications --- The proxy can&apos;t be reached.  Check your proxy settings, and check that the proxy is available.</source>
        <translation>لا يمكن الوصول إلى البروكسي. تحقق من إعدادات خادم البروكسي، وتحقق من أن الخادم متوفر.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="288"/>
        <source>ClientNotifications --- Can&apos;t reach the VPN server.  Please check your connection.</source>
        <translation>لا يمكن الوصول إلى خادم VPN.  يرجى التحقق من اتصالك.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="297"/>
        <source>ClientNotifications --- RECONNECT NEEDED</source>
        <translation>يجب إعادة الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="298"/>
        <source>ClientNotifications --- Reconnect to apply settings.</source>
        <translation>أعد الاتصال لتطبيق التغييرات.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="299"/>
        <source>ClientNotifications --- Some settings changes won&apos;t take effect until the next time you connect. Click to reconnect now.</source>
        <translation>لن تسري بعض تغييرات الإعدادات إلا عند اتصالك في المرة التالية. اضغط لإعادة الاتصال الآن.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="319"/>
        <source>ClientNotifications --- Connected using %1 port %2.</source>
        <extracomment>Message when the client automatically uses a transport other than the user&apos;s chosen transport (because the user&apos;s settings did not work). &quot;Connected&quot; means the client is currently connected right now using this setting. %1 is the protocol used (&quot;UDP&quot; or &quot;TCP&quot;), and %2 is the port number. For example: &quot;UDP port 8080&quot; or &quot;TCP port 443&quot;.</extracomment>
        <translation>تم الاتصال بـ %1 منفذ %2.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="326"/>
        <source>ClientNotifications --- Try Alternate Settings is enabled.  The server could not be reached on %1 port %2, so %3 port %4 was used instead.</source>
        <extracomment>Detailed message when the client automatically uses an alternate transport. &quot;%1 port %2&quot; refers to the chosen transport, and &quot;%3 port %4&quot; refers to the actual transport; for example &quot;TCP port 443&quot; or &quot;UDP port 8080&quot;. The &quot;Try Alternate Settings&quot; setting is on the Connection page.</extracomment>
        <translation>تم تمكين تجربة الإعدادات البديلة. تعذر الوصول إلى الخادم على %1 منفذ %2، لذا تم استخدام %3 منفذ %4 بدلاً من ذلك.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="340"/>
        <source>ClientNotifications --- Subscription expires in %1 days.</source>
        <translation>ينتهي الاشتراك خلال %1 أيام.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="343"/>
        <source>ClientNotifications --- Renew</source>
        <translation>تجديد</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="357"/>
        <source>ClientNotifications --- Unable to reach login server.</source>
        <extracomment>Dashboard notification for being unable to reach our main API server in order to authenticate the user&apos;s account. The phrase should convey that the problem is network related and that we are merely offline or &quot;out of touch&quot; rather than there being any account problem.</extracomment>
        <translation>تعذر الوصول لخادم تسجيل الدخول.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="361"/>
        <source>ClientNotifications --- Your account details are unavailable, but you may still be able to connect to the VPN.</source>
        <extracomment>Infotip to explain to the user that a login authentication failure is not necessarily a critical problem, but that the app will have reduced functionality until this works.</extracomment>
        <translation>تفاصيل حسابك غير متاحة، ولكن لا يزال بإمكانك الاتصال بخدمة VPN.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="365"/>
        <source>ClientNotifications --- Retry</source>
        <translation>إعادة المحاولة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="373"/>
        <source>ClientNotifications --- Private Internet Access was updated.</source>
        <translation>تم تحديث Private Internet Access.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="376"/>
        <source>ClientNotifications --- See what&apos;s new</source>
        <translation>اعرف ما هو الجديد</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ClientNotifications.qml" line="393"/>
        <source>ClientNotifications --- The application quit unexpectedly. Your VPN connection was preserved.</source>
        <extracomment>Indicates that Private Internet Access had previously crashed or otherwise stopped unexpectedly - shown the next time the user starts the app.</extracomment>
        <translation>تم إنهاء التطبيق بشكل غير متوقع. تم الاحتفاظ باتصال VPN.</translation>
    </message>
</context>
<context>
    <name>ConnectButton</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ConnectButton.qml" line="93"/>
        <source>ConnectButton --- Toggle connection</source>
        <extracomment>Screen reader annotation for the Connect button (the large &quot;power symbol&quot; button). Used for all states of the Connect button.</extracomment>
        <translation>تبديل الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ConnectButton.qml" line="99"/>
        <source>ConnectButton --- Resume from Snooze and reconnect, currently snoozing and disconnected</source>
        <extracomment>Description of the Connect button when connection is &quot;Snoozed&quot; meaning the connection is temporarily disconnected</extracomment>
        <translation>حاليًا في غفوة وغير متصل، قم بالاستئناف من الغفوة وإعادة الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ConnectButton.qml" line="106"/>
        <source>ConnectButton --- Connect to VPN, error has occurred</source>
        <extracomment>Description of the Connect button in the &quot;error&quot; state. This indicates that an error occurred recently.</extracomment>
        <translation>اتصال بخدمة VPN، حدث خطأ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ConnectButton.qml" line="110"/>
        <source>ConnectButton --- Connect to VPN</source>
        <extracomment>Description of the Connect button in the normal &quot;disconnected&quot; state</extracomment>
        <translation>اتصال بخدمة VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ConnectButton.qml" line="115"/>
        <source>ConnectButton --- Disconnect from VPN, connecting</source>
        <extracomment>Description of the Connect button when a connection is ongoing (clicking the button in this state disconnects, i.e. aborts the ongoing connection)</extracomment>
        <translation>قطع الاتصال من VPN، جارٍ الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ConnectButton.qml" line="118"/>
        <source>ConnectButton --- Disconnect from VPN</source>
        <extracomment>Description of the Connect button in the normal &quot;connected&quot; state</extracomment>
        <translation>قطع الاتصال من VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ConnectButton.qml" line="123"/>
        <source>ConnectButton --- Disconnect from VPN, disconnecting</source>
        <extracomment>Description of the Connect button while currently disconnecting. Clicking the button in this state still tries to disconnect (which has no real effect since it is already disconnecting).</extracomment>
        <translation>قطع الاتصال من VPN، جارٍ قطع الاتصال</translation>
    </message>
</context>
<context>
    <name>ConnectPage</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ConnectPage.qml" line="80"/>
        <source>ConnectPage --- Connect page</source>
        <extracomment>Screen reader annotation for Connect page. This describes the entire page that contains the Connect button and tiles.</extracomment>
        <translation>صفحة الاتصال</translation>
    </message>
</context>
<context>
    <name>ConnectionPage</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="26"/>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="196"/>
        <source>ConnectionPage --- Default</source>
        <translation>الافتراضي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="60"/>
        <source>ConnectionPage --- Connection Type</source>
        <translation>نوع الاتصال</translation>
    </message>
    <message>
    <source>ConnectionPage --- The Shadowsocks proxy setting requires TCP.</source>
        <translation>يتطلب إعداد وكيل Shadowsocks بروتوكول TCP.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="72"/>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="77"/>
        <source>ConnectionPage --- Remote Port</source>
        <translation>المنفذ البعيد</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="83"/>
        <source>ConnectionPage --- Local Port</source>
        <translation>المنفذ المحلي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="95"/>
        <source>ConnectionPage --- Auto</source>
        <translation>تلقائي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="292"/>
        <source>ConnectionPage --- Configuration Method</source>
        <translation>طريقة التكوين</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="299"/>
        <source>ConnectionPage --- DHCP</source>
        <extracomment>&quot;DHCP&quot; refers to Dynamic Host Configuration Protocol, a network configuration technology. This probably is not translated for most languages.</extracomment>
        <translation>DHCP</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="303"/>
        <source>ConnectionPage --- Static</source>
        <extracomment>&quot;Static&quot; is an alternative to DHCP - instead of using dynamic configuration on the network adapter, it is configured with static addresses.</extracomment>
        <translation>ثابت</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="308"/>
        <source>ConnectionPage --- Determines how addresses are configured on the TAP adapter.  If you have trouble connecting, a different method may be more reliable.</source>
        <extracomment>Description of the configuration method choices for Windows. This should suggest that the only reason to change this setting is if you have trouble connecting.</extracomment>
        <translation>يحدد كيفية تكوين العناوين على محول TAP. إذا كنت تواجه مشكلة في الاتصال، فقد تكون الطريقة الأخرى أكثر موثوقية.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="244"/>
        <source>ConnectionPage --- Data Encryption</source>
        <translation>تشفير البيانات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="116"/>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="251"/>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="265"/>
        <source>ConnectionPage --- None</source>
        <translation>لا شيء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="253"/>
        <source>ConnectionPage --- Warning: Your traffic is sent unencrypted and is vulnerable to eavesdropping.</source>
        <translation>تحذير: يتم إرسال حركة مرورك بدون تشفير وهي بذلك عرضة للتنصت.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="260"/>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="270"/>
        <source>ConnectionPage --- Data Authentication</source>
        <translation>مصادقة البيانات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="267"/>
        <source>ConnectionPage --- Warning: Your traffic is unauthenticated and is vulnerable to manipulation.</source>
        <translation>تحذير: حركة مرورك غير مصادقة وهي بذلك عرضة للتلاعب.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="279"/>
        <source>ConnectionPage --- Handshake</source>
        <translation>المصافحة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="289"/>
        <source>ConnectionPage --- This handshake relies on an Elliptic Curve endorsed by US standards bodies.</source>
        <translation>تعتمد هذه المصافحة على منحنى إهليجي معتمد من هيئات المعايير الأمريكية.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="319"/>
        <source>ConnectionPage --- Use Small Packets</source>
        <translation>استخدم حزم صغيرة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="323"/>
        <source>ConnectionPage --- Set a smaller MTU for the VPN connection. This can result in lower transfer speeds but improved reliability on poor connections.</source>
        <translation>قم بتعيين وحدة إرسال قصوى (MTU) أصغر لاتصال VPN. هذا يمكن أن يؤدي إلى انخفاض سرعات النقل ولكن سيحسّن من استقرار الاتصالات منخفضة الجودة.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="327"/>
        <source>ConnectionPage --- Try Alternate Settings</source>
        <translation>تجربة الإعدادات البديلة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="336"/>
        <source>ConnectionPage --- If the connection type and remote port above do not work, try other settings automatically.</source>
        <extracomment>Tip for the automatic transport setting. Refers to the &quot;Connection Type&quot; and &quot;Remote Port&quot; settings above on the Connection page.</extracomment>
        <translation>إذا لم يعمل نوع الاتصال والمنفذ البعيد أعلاه، جرب إعدادات أخرى تلقائيًا.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="344"/>
        <source>ConnectionPage --- Alternate settings can&apos;t be used when a proxy is configured.</source>
        <extracomment>Tip used for the automatic transport setting when a proxy is configured - the two settings can&apos;t be used together.</extracomment>
        <translation>لا يمكن استخدام الإعدادات البديلة عند وجود تكوين بروكسي.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="357"/>
        <source>ConnectionPage --- What do these settings mean?</source>
        <translation>ماذا تعني هذه الإعدادات؟</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="99"/>
        <source>ConnectionPage --- Proxy</source>
        <translation>بروكسي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="119"/>
        <source>ConnectionPage --- SOCKS5 Proxy...</source>
        <translation>بروكسي SOCKS5...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="150"/>
        <source>ConnectionPage --- SOCKS5 Proxy</source>
        <translation>بروكسي SOCKS5</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="174"/>
        <source>ConnectionPage --- Server</source>
        <translation>الخادم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="178"/>
        <source>ConnectionPage --- Server IP Address</source>
        <extracomment>The IP address of the SOCKS proxy server to use when connecting. Labeled with &quot;IP Address&quot; to indicate that it can&apos;t be a hostname.</extracomment>
        <translation>عنوان IP للخادم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="193"/>
        <source>ConnectionPage --- Port</source>
        <translation>المنفذ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="202"/>
        <source>ConnectionPage --- User (optional)</source>
        <translation>المستخدم (اختياري)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/ConnectionPage.qml" line="209"/>
        <source>ConnectionPage --- Password (optional)</source>
        <translation>كلمة المرور (اختياري)</translation>
    </message>
</context>
<context>
    <name>DaemonAccount</name>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonAccount.qml" line="28"/>
        <source>DaemonAccount --- Deactivated</source>
        <translation>غير نشط</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonAccount.qml" line="30"/>
        <source>DaemonAccount --- One Month Plan</source>
        <translation>خطة شهر واحد</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonAccount.qml" line="31"/>
        <source>DaemonAccount --- Three Month Plan</source>
        <translation>خطة ثلاثة أشهر</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonAccount.qml" line="32"/>
        <source>DaemonAccount --- Six Month Plan</source>
        <translation>خطة ستة أشهر</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonAccount.qml" line="33"/>
        <source>DaemonAccount --- One Year Plan</source>
        <translation>خطة سنة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonAccount.qml" line="34"/>
        <source>DaemonAccount --- Two Year Plan</source>
        <translation>خطة سنتان</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonAccount.qml" line="35"/>
        <source>DaemonAccount --- Three Year Plan</source>
        <translation>خطة ثلاث سنوات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonAccount.qml" line="36"/>
        <source>DaemonAccount --- Trial</source>
        <translation>الفترة التجريبية</translation>
    </message>
</context>
<context>
    <name>DaemonData</name>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="36"/>
        <source>DaemonData --- UAE</source>
        <translation>الإمارات العربية المتحدة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="37"/>
        <source>DaemonData --- AU Sydney</source>
        <translation>أستراليا، سيدني</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="38"/>
        <source>DaemonData --- AU Melbourne</source>
        <translation>أستراليا، ملبورن</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="39"/>
        <source>DaemonData --- AU Perth</source>
        <translation>أستراليا، برث</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="40"/>
        <source>DaemonData --- Austria</source>
        <translation>النمسا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="41"/>
        <source>DaemonData --- Belgium</source>
        <translation>بلجيكا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="42"/>
        <source>DaemonData --- Brazil</source>
        <translation>البرازيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="43"/>
        <source>DaemonData --- CA Montreal</source>
        <translation>كندا، مونتريال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="44"/>
        <source>DaemonData --- CA Toronto</source>
        <translation>كندا، تورونتو</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="45"/>
        <source>DaemonData --- CA Vancouver</source>
        <translation>كندا، فانكوفر</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="46"/>
        <source>DaemonData --- Czech Republic</source>
        <translation>جمهورية التشيك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="47"/>
        <source>DaemonData --- DE Berlin</source>
        <translation>ألمانيا، برلين</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="48"/>
        <source>DaemonData --- Denmark</source>
        <translation>الدنمارك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="49"/>
        <source>DaemonData --- Finland</source>
        <translation>فنلندا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="50"/>
        <source>DaemonData --- France</source>
        <translation>فرنسا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="51"/>
        <source>DaemonData --- DE Frankfurt</source>
        <translation>ألمانيا، فرانكفورت</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="52"/>
        <source>DaemonData --- Hong Kong</source>
        <translation>هونغ كونغ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="53"/>
        <source>DaemonData --- Hungary</source>
        <translation>المجر</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="54"/>
        <source>DaemonData --- India</source>
        <translation>الهند</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="55"/>
        <source>DaemonData --- Ireland</source>
        <translation>إيرلندا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="56"/>
        <source>DaemonData --- Israel</source>
        <translation>إسرائيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="57"/>
        <source>DaemonData --- Italy</source>
        <translation>إيطاليا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="58"/>
        <source>DaemonData --- Japan</source>
        <translation>اليابان</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="59"/>
        <source>DaemonData --- Luxembourg</source>
        <translation>لوكسمبورغ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="60"/>
        <source>DaemonData --- Mexico</source>
        <translation>المكسيك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="61"/>
        <source>DaemonData --- Netherlands</source>
        <translation>هولندا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="62"/>
        <source>DaemonData --- Norway</source>
        <translation>النرويج</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="63"/>
        <source>DaemonData --- New Zealand</source>
        <translation>نيوزيلندا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="64"/>
        <source>DaemonData --- Poland</source>
        <translation>بولندا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="65"/>
        <source>DaemonData --- Romania</source>
        <translation>رومانيا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="66"/>
        <source>DaemonData --- Singapore</source>
        <translation>سنغافورة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="67"/>
        <source>DaemonData --- Spain</source>
        <translation>إسبانيا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="68"/>
        <source>DaemonData --- Sweden</source>
        <translation>السويد</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="69"/>
        <source>DaemonData --- Switzerland</source>
        <translation>سويسرا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="70"/>
        <source>DaemonData --- Turkey</source>
        <translation>تركيا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="71"/>
        <source>DaemonData --- UK London</source>
        <translation>المملكة المتحدة، لندن</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="72"/>
        <source>DaemonData --- UK Manchester</source>
        <translation>المملكة المتحدة، مانشستر</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="73"/>
        <source>DaemonData --- UK Southampton</source>
        <translation>المملكة المتحدة، ساوثامبتون</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="74"/>
        <source>DaemonData --- US East</source>
        <translation>الولايات المتحدة، الشرق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="75"/>
        <source>DaemonData --- US West</source>
        <translation>الولايات المتحدة، الغرب</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="76"/>
        <source>DaemonData --- US Atlanta</source>
        <translation>الولايات المتحدة، أتلانتا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="77"/>
        <source>DaemonData --- US California</source>
        <translation>الولايات المتحدة، كاليفورنيا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="78"/>
        <source>DaemonData --- US Chicago</source>
        <translation>الولايات المتحدة، شيكاغو</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="79"/>
        <source>DaemonData --- US Denver</source>
        <translation>الولايات المتحدة، دنفر</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="80"/>
        <source>DaemonData --- US Florida</source>
        <translation>الولايات المتحدة، فلوريدا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="81"/>
        <source>DaemonData --- US Houston</source>
        <translation>الولايات المتحدة، هيوستن</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="82"/>
        <source>DaemonData --- US Las Vegas</source>
        <translation>الولايات المتحدة، لاس فيغاس</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="83"/>
        <source>DaemonData --- US New York City</source>
        <translation>الولايات المتحدة، نيويورك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="84"/>
        <source>DaemonData --- US Seattle</source>
        <translation>الولايات المتحدة، سياتل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="85"/>
        <source>DaemonData --- US Silicon Valley</source>
        <translation>الولايات المتحدة، سيليكون فالي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="86"/>
        <source>DaemonData --- US Texas</source>
        <translation>الولايات المتحدة، تكساس</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="87"/>
        <source>DaemonData --- US Washington DC</source>
        <translation>الولايات المتحدة، واشنطن العاصمة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="88"/>
        <source>DaemonData --- South Africa</source>
        <translation>جنوب أفريقيا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="94"/>
        <source>DaemonData --- Germany</source>
        <translation>ألمانيا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="95"/>
        <source>DaemonData --- Canada</source>
        <translation>كندا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="96"/>
        <source>DaemonData --- United States</source>
        <translation>الولايات المتحدة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="97"/>
        <source>DaemonData --- Australia</source>
        <translation>أستراليا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/daemon/DaemonData.qml" line="98"/>
        <source>DaemonData --- United Kingdom</source>
        <translation>المملكة المتحدة</translation>
    </message>
</context>
<context>
    <name>DashboardPopup</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/DashboardPopup.qml" line="78"/>
        <source>DashboardPopup --- PIA Dashboard</source>
        <extracomment>Title of the dashboard window (the main UI that users interact with. This isn&apos;t normally shown, but it is used by screen readers, and it is occasionally used by tools on Windows that list open application windows. &quot;PIA&quot; stands for Private Internet Access. We refer to this window as the &quot;dashboard&quot;, but this term doesn&apos;t currently appear elsewhere in the product.</extracomment>
        <translation>لوحة معلومات PIA</translation>
    </message>
</context>
<context>
    <name>DashboardWindow</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/DashboardWindow.qml" line="55"/>
        <source>DashboardWindow --- Connect</source>
        <translation>اتصال</translation>
    </message>
</context>
<context>
    <name>DialogMessage</name>
    <message>
        <location filename="../../../../../client/res/components/common/DialogMessage.qml" line="28"/>
        <source>DialogMessage --- Information</source>
        <extracomment>Screen reader annotation for the &quot;info&quot; icon in dialog messages</extracomment>
        <translation>معلومات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/DialogMessage.qml" line="30"/>
        <source>DialogMessage --- Warning</source>
        <extracomment>Screen reader annotation for the &quot;warning&quot; icon in dialog messages</extracomment>
        <translation>تحذير</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/DialogMessage.qml" line="32"/>
        <source>DialogMessage --- Error</source>
        <extracomment>Screen reader annotation for the &quot;error&quot; icon in dialog messages</extracomment>
        <translation>خطأ</translation>
    </message>
</context>
<context>
    <name>Error</name>
    <message>
        <location filename="../../../../../common/src/builtin/error.cpp" line="36"/>
        <source>Error --- Unknown error</source>
        <translation>خطأ غير معروف</translation>
    </message>
    <message>
        <location filename="../../../../../common/src/builtin/error.cpp" line="37"/>
        <source>Error --- System error %1: %2</source>
        <translation>خطأ في النظام %1: %2</translation>
    </message>
    <message>
        <location filename="../../../../../common/src/builtin/error.cpp" line="37"/>
        <source>Error --- System error %1 inside %3: %2</source>
        <translation>خطأ في النظام %1 من الداخل %3: %2</translation>
    </message>
    <message>
        <location filename="../../../../../common/src/builtin/error.cpp" line="38"/>
        <source>Error --- Unknown error code %1</source>
        <translation>خطأ غير معروف رمز %1</translation>
    </message>
    <message>
        <location filename="../../../../../common/src/builtin/error.cpp" line="46"/>
        <source>Error --- No additional information available.</source>
        <translation>لا توجد معلومات إضافية متاحة.</translation>
    </message>
</context>
<context>
    <name>ExpandButton</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ExpandButton.qml" line="71"/>
        <source>ExpandButton --- Collapse</source>
        <extracomment>Screen reader annotations for the Expand button at the bottom of the Connect page, which either expands or collapses the dashboard to show/hide the extra tiles. This title should be a brief name (typically one or two words) of the action that the button will take.</extracomment>
        <translation>طي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ExpandButton.qml" line="71"/>
        <source>ExpandButton --- Expand</source>
        <translation>توسيع</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ExpandButton.qml" line="77"/>
        <source>ExpandButton --- Hide extra tiles</source>
        <extracomment>Screen reader annotations for the Expand button at the bottom of the Connect page, which either expands or collapses the dashboard to show/hide the extra tiles. This title should be a short description (typically a few words) indicating that the button will show or hide the extra tiles.</extracomment>
        <translation>إخفاء اللوحات الإضافية</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/ExpandButton.qml" line="77"/>
        <source>ExpandButton --- Show extra tiles</source>
        <translation>إظهار اللوحات الإضافية</translation>
    </message>
</context>
<context>
    <name>GeneralPage</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="21"/>
        <source>GeneralPage --- Launch on System Startup</source>
        <translation>تشغيل عند بدء النظام</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="40"/>
        <source>GeneralPage --- Connect on Launch</source>
        <translation>اتصال عند البدء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="44"/>
        <source>GeneralPage --- Show Desktop Notifications</source>
        <translation>إظهار إشعارات سطح المكتب</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="49"/>
        <source>GeneralPage --- Language</source>
        <translation>اللغة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="65"/>
        <source>GeneralPage --- Theme</source>
        <translation>النُسق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="68"/>
        <source>GeneralPage --- Dark</source>
        <translation>داكن</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="69"/>
        <source>GeneralPage --- Light</source>
        <translation>فاتح</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="76"/>
        <source>GeneralPage --- Tray Icon Style</source>
        <extracomment>This setting allows the user to choose a style for the icon shown in the system tray / notification area. It should use the typical desktop terminology for the &quot;tray&quot;.</extracomment>
        <translation>نمط أيقونة علبة النظام</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="81"/>
        <source>GeneralPage --- The &apos;Auto&apos; setting chooses an icon based on your desktop theme.</source>
        <translation>يختار إعداد &quot;تلقائي&quot; إحدى الأيقونات استنادًا إلى نُسق سطح المكتب.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="86"/>
        <extracomment>icon-theme</extracomment>
<source>GeneralPage -- icon-theme --- Auto</source>
        <comment>icon-theme</comment>
        <translation>تلقائي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="87"/>
        <extracomment>icon-theme</extracomment>
<source>GeneralPage -- icon-theme --- Light</source>
        <comment>icon-theme</comment>
        <translation>فاتح</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="88"/>
        <extracomment>icon-theme</extracomment>
<source>GeneralPage -- icon-theme --- Dark</source>
        <comment>icon-theme</comment>
        <translation>داكن</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="89"/>
        <extracomment>icon-theme</extracomment>
<source>GeneralPage -- icon-theme --- Colored</source>
        <comment>icon-theme</comment>
        <translation>ملونة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="90"/>
        <extracomment>icon-theme</extracomment>
<source>GeneralPage -- icon-theme --- Classic</source>
        <comment>icon-theme</comment>
        <translation>كلاسيكي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="107"/>
        <source>GeneralPage --- Dashboard Appearance</source>
        <extracomment>Setting controlling how the dashboard is displayed - either as a popup attached to the system tray or as an ordinary window.</extracomment>
        <translation>مظهر لوحة المعلومات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="112"/>
        <source>GeneralPage --- Attached to Tray</source>
        <extracomment>Setting value indicating that the dashboard is a popup attached to the system tray.</extracomment>
        <translation>ملحق بعلبة النظام</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="114"/>
        <source>GeneralPage --- Window</source>
        <extracomment>Setting value indicating that the dashboard is an ordinary window</extracomment>
        <translation>نافذة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="118"/>
        <source>GeneralPage --- Attached mode may not work with all desktop environments, and it requires a system tray. If you can&apos;t find the dashboard, start Private Internet Access again to show it, and switch back to Window mode in Settings.</source>
        <translation>قد لا يعمل الوضع الملحق مع جميع بيئات سطح المكتب، ويتطلب وجود علبة النظام. إذا لم تتمكن من العثور على لوحة المعلومات، ابدأ Private Internet Access مرة أخرى لإظهارها، ثم عُد إلى وضع النافذة في الإعدادات.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="132"/>
        <source>GeneralPage --- Reset All Settings</source>
        <translation>إعادة تعيين كل الإعدادات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/GeneralPage.qml" line="135"/>
        <source>GeneralPage --- Reset all settings to their default values?</source>
        <translation>هل تريد إعادة تعيين كل الإعدادات إلى القيم الافتراضية؟</translation>
    </message>
</context>
<context>
    <name>HeaderBar</name>
    <message>
        <location filename="../../../../../client/res/components/common/Messages.qml" line="10"/>
        <source>HeaderBar --- Alpha pre-release</source>
        <extracomment>Screen reader annotation for the &quot;Alpha&quot; banner shown in alpha prerelease builds</extracomment>
        <translation>إصدار المرحلة قبل ألفا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/Messages.qml" line="13"/>
        <source>HeaderBar --- Beta pre-release</source>
        <extracomment>Screen reader annotation for the &quot;Beta&quot; banner shown in beta prerelease builds</extracomment>
        <translation>إصدار تجريبي قبل الإصدار الرسمي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="184"/>
        <source>HeaderBar --- Back</source>
        <extracomment>Screen reader annotation for the &quot;Back&quot; button in the header, which returns to the previous page. Should use the typical term for a &quot;back&quot; button in a dialog flow, wizard, etc.</extracomment>
        <translation>عودة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="261"/>
        <source>HeaderBar --- RESUMING</source>
        <translation>استئناف</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="263"/>
        <source>HeaderBar --- SNOOZING</source>
        <translation>دخول غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="265"/>
        <source>HeaderBar --- SNOOZED</source>
        <translation>في غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="276"/>
        <source>HeaderBar --- CONNECTING</source>
        <translation>جارٍ الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="278"/>
        <source>HeaderBar --- DISCONNECTING</source>
        <translation>جارٍ قطع الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="280"/>
        <source>HeaderBar --- CONNECTED</source>
        <translation>متصل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="337"/>
        <source>HeaderBar --- Menu</source>
        <extracomment>Screen reader annotation for the &quot;Menu&quot; button in the header. This button displays a popup menu.</extracomment>
        <translation>القائمة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="344"/>
        <source>HeaderBar --- Menu, update available</source>
        <extracomment>Screen reader annotation for the &quot;Menu&quot; button in the header when it displays the &quot;update available&quot; badge. The button still displays the normal popup menu, and the &quot;Menu&quot; translation should come first since that&apos;s its action. &quot;Update available&quot; is added as an additional description of the update badge.</extracomment>
        <translation>القائمة، يوجد تحديث متاح</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="399"/>
        <source>HeaderBar --- Settings</source>
        <translation>الإعدادات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="403"/>
        <source>HeaderBar --- Logout</source>
        <translation>تسجيل الخروج</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/HeaderBar.qml" line="408"/>
        <source>HeaderBar --- Quit</source>
        <translation>خروج</translation>
    </message>
</context>
<context>
    <name>HelpPage</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="31"/>
        <source>HelpPage --- Version</source>
        <translation>الإصدار</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="57"/>
        <source>HelpPage --- Client:</source>
        <translation>العميل:</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="72"/>
        <source>HelpPage --- Daemon:</source>
        <translation>العفريت:</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="87"/>
        <source>HelpPage --- Changelog</source>
        <translation>سجل التغييرات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="96"/>
        <source>HelpPage --- Quick Tour</source>
        <extracomment>This link displays the tour that users see initially after installation.</extracomment>
        <translation>جولة سريعة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="106"/>
        <source>HelpPage --- Receive Beta Updates</source>
        <translation>تلقي تحديثات الإصدار التجريبي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="107"/>
        <source>HelpPage --- Join our beta program to test new features and provide feedback.</source>
        <translation>انضم إلى برنامجنا التجريبي لاختبار الميزات الجديدة وتقديم التعليقات.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="159"/>
        <source>HelpPage --- Support</source>
        <translation>الدعم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="167"/>
        <source>HelpPage --- Disable Accelerated Graphics</source>
        <translation>تعطيل الرسوميات المسرّعة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="176"/>
        <source>HelpPage --- Restart Private Internet Access to apply this setting</source>
        <translation>أعد تشغيل Private Internet Access لتطبيق هذا الإعداد</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="184"/>
        <source>HelpPage --- Accelerated graphics reduce CPU usage and enable graphical effects, but can cause issues with certain graphics cards or drivers.</source>
        <translation>تعمل الرسومات المسرَّعة على تقليل استخدام المعالج وتمكين التأثيرات الرسومية، ولكن يمكن أن تسبب مشكلات في بطاقات الرسوميات أو برامج التشغيل.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="188"/>
        <source>HelpPage --- Enable Debug Logging</source>
        <translation>تمكين تسجيل تصحيح الأخطاء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="189"/>
        <source>HelpPage --- Save debug logs which can be submitted to technical support to help troubleshoot problems.</source>
        <translation>حفظ سجلات تصحيح الأخطاء التي يمكن إرسالها إلى الدعم الفني للمساعدة في استكشاف المشكلات وإصلاحها.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="205"/>
        <source>HelpPage --- Submit Debug Logs</source>
        <translation>إرسال سجلات تصحيح الأخطاء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="211"/>
        <source>HelpPage --- Support Portal</source>
        <translation>بوابة الدعم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="226"/>
        <source>HelpPage --- Maintenance</source>
        <translation>الصيانة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="237"/>
        <source>HelpPage --- Reinstalling Network Adapter...</source>
        <translation>جارٍ إعادة تثبيت بطاقة الشبكة...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="239"/>
        <source>HelpPage --- Waiting for Disconnect...</source>
        <translation>قيد انتظار قطع الاتصال...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="241"/>
        <source>HelpPage --- Reinstall Network Adapter</source>
        <translation>إعادة تثبيت بطاقة الشبكة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="251"/>
        <source>HelpPage --- The network adapter cannot be reinstalled while connected. Disconnect and reinstall now?</source>
        <translation>لا يمكن إعادة تثبيت بطاقة الشبكة وهي قيد الاتصال. هل تريد قطع الاتصال وإعادة التثبيت الآن؟</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="251"/>
        <source>HelpPage --- Disconnect needed</source>
        <translation>يجب قطع الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="284"/>
        <source>HelpPage --- The network adapter has been successfully reinstalled.</source>
        <translation>تم إعادة تثبيت بطاقة الشبكة بنجاح.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="287"/>
        <source>HelpPage --- The network adapter has been successfully reinstalled. You may need to reboot your system.</source>
        <translation>تم إعادة تثبيت بطاقة الشبكة بنجاح. قد تحتاج إلى إعادة تشغيل النظام.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="290"/>
        <source>HelpPage --- There was an error while attempting to reinstall the network adapter.</source>
        <translation>حدث خطأ أثناء محاولة إعادة تثبيت بطاقة الشبكة.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="300"/>
        <source>HelpPage --- Failed to run command in terminal. Please install a terminal compatible with x-terminal-emulator.</source>
        <extracomment>&quot;Terminal&quot; refers to a terminal emulator in the Linux build, such as xterm, GNOME Terminal, Konsole, etc. This should use the typical desktop terminology.</extracomment>
        <translation>تعذّر تشغيل الأمر في المحطة الطرفية. يرجى تثبيت محطة طرفية متوافقة مع x‑terminal‑emulator.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="302"/>
        <source>HelpPage --- Unable to open terminal</source>
        <translation>تعذّر فتح المحطة الطرفية</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="331"/>
        <source>HelpPage --- Reinstalling Split Tunnel Filter...</source>
        <translation>جارٍ تثبيت مرشح النفق المقسم...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="332"/>
        <source>HelpPage --- Reinstall Split Tunnel Filter</source>
        <translation>إعادة تثبيت مرشح النفق المقسم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="371"/>
        <source>HelpPage --- The split tunnel filter was reinstalled.</source>
        <translation>تمت إعادة تثبيت مرشح النفق المقسم.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="394"/>
        <source>HelpPage --- Uninstall Private Internet Access</source>
        <translation>إزالة تثبيت Private Internet Access</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="402"/>
        <source>HelpPage --- Uninstall {{BRAND}}?</source>
        <translation>إزالة تثبيت {{BRAND}}؟</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="402"/>
        <source>HelpPage --- Uninstall</source>
        <translation>إزالة التثبيت</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/HelpPage.qml" line="444"/>
        <source>HelpPage --- v%1</source>
        <translation>الإصدار %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsMessages.qml" line="10"/>
        <source>HelpPage --- Reinstall successful</source>
        <translation>إعادة تثبيت ناجحة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsMessages.qml" line="12"/>
        <source>HelpPage --- Reinstall error</source>
        <translation>خطأ في إعادة التثبيت</translation>
    </message>
</context>
<context>
    <name>IPModule</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/IPModule.qml" line="20"/>
        <source>IPModule --- IP tile</source>
        <extracomment>Screen reader annotation for the tile displaying the IP addresses.</extracomment>
        <translation>لوحة IP</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/IPModule.qml" line="48"/>
        <source>IPModule --- IP</source>
        <translation>IP</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/IPModule.qml" line="74"/>
        <source>IPModule --- VPN IP</source>
        <translation>VPN IP</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/IPModule.qml" line="119"/>
        <source>IPModule --- Port forward</source>
        <extracomment>Screen reader annotation for the arrow graphic that represents the &quot;port forward&quot; status, which is enabled by the &quot;Port Forwarding&quot; setting.</extracomment>
        <translation>إعادة توجيه المنافذ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/IPModule.qml" line="162"/>
        <source>IPModule --- Failed</source>
        <extracomment>Port forward - label used in IP widget when request fails</extracomment>
        <translation>تعذَّرت العملية</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/IPModule.qml" line="165"/>
        <source>IPModule --- Not Available</source>
        <extracomment>Port forward - label used in IP widget when not available for this region</extracomment>
        <translation>غير متوفر</translation>
    </message>
</context>
<context>
    <name>LoginButton</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginButton.qml" line="163"/>
        <source>LoginButton --- LOG IN</source>
        <translation>تسجيل الدخول</translation>
    </message>
</context>
<context>
    <name>LoginPage</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginPage.qml" line="51"/>
        <source>LoginPage --- Invalid login</source>
        <translation>تسجيل دخول غير صالح</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginPage.qml" line="53"/>
        <source>LoginPage --- Too many attempts, try again in 1 hour</source>
        <translation>لقد قمت بمحاولات كثيرة، أعد المحاولة خلال ساعة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginPage.qml" line="55"/>
        <source>LoginPage --- Can&apos;t reach the server</source>
        <translation>تعذَر الوصول إلى الخادم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginPage.qml" line="84"/>
        <source>LoginPage --- Username</source>
        <translation>اسم المستخدم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginPage.qml" line="93"/>
        <source>LoginPage --- Use your normal username beginning with &apos;p&apos;.</source>
        <extracomment>Shown if the user attempts to login with the wrong account type. &apos;p&apos; refers to the letter prefix on the username; the p should be kept in Latin script. (Example user names are &quot;p0123456&quot;, &quot;p5858587&quot;).</extracomment>
        <translation>استخدم اسم المستخدم العادي الذي يبدأ بحرف &quot;p&quot;.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginPage.qml" line="106"/>
        <source>LoginPage --- Password</source>
        <translation>كلمة المرور</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginPage.qml" line="160"/>
        <source>LoginPage --- Forgot Password</source>
        <translation>نسيت كلمة المرور</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/login/LoginPage.qml" line="170"/>
        <source>LoginPage --- Buy Account</source>
        <translation>شراء حساب</translation>
    </message>
</context>
<context>
    <name>ModuleLoader</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/MovableModule.qml" line="265"/>
        <source>ModuleLoader --- Favorite tile</source>
        <extracomment>Screen reader annotation for the &quot;bookmark&quot; button on tiles. This behaves like a checkbox that can be toggled, i.e. &quot;this is a favorite tile&quot; - set to on/off.</extracomment>
        <translation>لوحة المفضلة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/MovableModule.qml" line="271"/>
        <source>ModuleLoader --- Remove %1 from favorites</source>
        <extracomment>Screen reader annotation for &apos;active&apos; tile bookmark button that will remove a tile from favorites. %1 is a tile name, like &quot;Performance tile&quot;, &quot;Account tile&quot;, etc.</extracomment>
        <translation>إزالة %1 من المفضلة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/MovableModule.qml" line="276"/>
        <source>ModuleLoader --- Add %1 to favorites</source>
        <extracomment>Screen reader annotation for &apos;inactive&apos; tile bookmark button that will add a tile to favorites. %1 is a tile name, like &quot;Performance tile&quot;, &quot;Account tile&quot;, etc.</extracomment>
        <translation>إضافة %1 إلى المفضلة</translation>
    </message>
</context>
<context>
    <name>NativeAcc::MoveButtonAttached</name>
    <message>
        <location filename="../../../../../client/src/nativeacc/buttons.cpp" line="81"/>
        <source>NativeAcc::MoveButtonAttached --- Moves the tile up</source>
        <extracomment>Screen reader description of the &quot;move up&quot; action used to move a tile up in the list. Grammatically, the implied subject is the accessibility action, &quot;[This action] moves the tile up&quot;.</extracomment>
        <translation>يحرك اللوحة لأعلى</translation>
    </message>
    <message>
        <location filename="../../../../../client/src/nativeacc/buttons.cpp" line="88"/>
        <source>NativeAcc::MoveButtonAttached --- Moves the tile down</source>
        <extracomment>Screen reader description of the &quot;move down&quot; action used to move a tile down in the list. Grammatically, the implied subject is the accessibility action, &quot;[This action] moves the tile down&quot;.</extracomment>
        <translation>يحرك اللوحة لأسفل</translation>
    </message>
    <message>
        <location filename="../../../../../client/src/nativeacc/buttons.cpp" line="100"/>
        <source>NativeAcc::MoveButtonAttached --- Move up</source>
        <extracomment>Screen reader annotation of the &quot;move up&quot; action used to move a tile up in the list. Should be a verb or short verb phrase.</extracomment>
        <translation>تحريك لأعلى</translation>
    </message>
    <message>
        <location filename="../../../../../client/src/nativeacc/buttons.cpp" line="106"/>
        <source>NativeAcc::MoveButtonAttached --- Move down</source>
        <extracomment>Screen reader annotation of the &quot;move down&quot; action used to move a tile down in the list. Should be a verb or short verb phrase.</extracomment>
        <translation>تحريك لأسفل</translation>
    </message>
</context>
<context>
    <name>NativeAcc::TableCellCheckButtonImpl</name>
    <message>
        <location filename="../../../../../client/src/nativeacc/tablecells.cpp" line="133"/>
        <source>NativeAcc::TableCellCheckButtonImpl --- checked</source>
        <extracomment>Value indicator for a toggle button in the &quot;checked&quot; state. Should use normal terminology for a check box or toggle button. (Screen reader annotation for &quot;Favorite region&quot; table cell button on Windows.)</extracomment>
        <translation>محددة</translation>
    </message>
    <message>
        <location filename="../../../../../client/src/nativeacc/tablecells.cpp" line="141"/>
        <source>NativeAcc::TableCellCheckButtonImpl --- unchecked</source>
        <extracomment>Value indicator for a toggle button in the &quot;unchecked&quot; state. Should use normal terminology for a check box or toggle button. (Screen reader annotation for &quot;Favorite region&quot; table cell button on Windows.)</extracomment>
        <translation>غير محددة</translation>
    </message>
</context>
<context>
    <name>NativeAcc::ValueTextAttached</name>
    <message>
        <location filename="../../../../../client/src/nativeacc/valuetext.cpp" line="65"/>
        <source>NativeAcc::ValueTextAttached --- Copies the value</source>
        <extracomment>Screen reader description of the &quot;copy&quot; action for IP address/port fields, etc. &quot;Copies&quot; refers to copying to the system clipboard and should use the OS&apos;s normal terminology. Grammatically, the implied subject is the accessibility action, &quot;[This action] copies the value [to the clipboard]&quot;.</extracomment>
        <translation>ينسخ القيمة</translation>
    </message>
    <message>
        <location filename="../../../../../client/src/nativeacc/valuetext.cpp" line="78"/>
        <source>NativeAcc::ValueTextAttached --- Copy</source>
        <extracomment>Screen reader annotation to describe the &quot;copy&quot; action on the IP address and port fields, etc. Copies the text to the clipboard, should be a verb or short verb phrase.</extracomment>
        <translation>نسخ</translation>
    </message>
</context>
<context>
    <name>NavigationDot</name>
    <message>
        <location filename="../../../../../client/res/components/onboarding/NavigationDot.qml" line="26"/>
        <source>NavigationDot --- Page %1</source>
        <extracomment>Screen reader annotation for the navigation dots in the Quick Tour. These indicate pages the user can navigate to; &quot;%1&quot; is a page index from 1 to 3.</extracomment>
        <translation>صفحة %1</translation>
    </message>
</context>
<context>
    <name>Notification</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/notifications/Notification.qml" line="26"/>
        <source>Notification --- Info</source>
        <extracomment>Screen reader annotation for the &quot;info&quot; icon used for messages</extracomment>
        <translation>معلومات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/notifications/Notification.qml" line="28"/>
        <source>Notification --- Warning</source>
        <extracomment>Screen reader annotation for the &quot;warning&quot; icon used for messages</extracomment>
        <translation>تحذير</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/notifications/Notification.qml" line="30"/>
        <source>Notification --- Error</source>
        <extracomment>Screen reader annotation for the &quot;error&quot; icon used for messages</extracomment>
        <translation>خطأ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/notifications/Notification.qml" line="59"/>
        <source>Notification --- %1: %2</source>
        <extracomment>Screen reader annotation used for Connect page message group, such as &quot;Warning: Killswitch is enabled&quot;. &quot;%1&quot; is a severity name (&quot;Info&quot;/ &quot;Warning&quot;/&quot;Error&quot;), and &quot;%2&quot; is a status message.</extracomment>
        <translation>%1: %2</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/notifications/Notification.qml" line="108"/>
        <source>Notification --- Dismiss message: %1</source>
        <extracomment>Screen reader annotation for the &quot;X&quot; button on dismissible messages. This removes the message until it triggers again. &quot;%1&quot; is a message, such as &quot;Killswitch enabled.&quot;</extracomment>
        <translation>تجاهل الرسالة: %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/notifications/Notification.qml" line="205"/>
        <source>Notification --- Stop</source>
        <extracomment>Screen reader annotation for the square &quot;Stop&quot; button on the message used to download an update.</extracomment>
        <translation>إيقاف</translation>
    </message>
</context>
<context>
    <name>OnboardingWindow</name>
    <message>
        <location filename="../../../../../client/res/components/onboarding/OnboardingWindow.qml" line="13"/>
        <source>OnboardingWindow --- Quick Tour</source>
        <translation>جولة سريعة</translation>
    </message>
</context>
<context>
    <name>OverlayDialog</name>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="42"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- OK</source>
        <comment>dialog button</comment>
        <translation>حسنا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="43"/>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="45"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Cancel</source>
        <comment>dialog button</comment>
        <translation>إلغاء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="44"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Save</source>
        <comment>dialog button</comment>
        <translation>حفظ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="46"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Close</source>
        <comment>dialog button</comment>
        <translation>إغلاق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="47"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Discard</source>
        <comment>dialog button</comment>
        <translation>تجاهل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="48"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Apply</source>
        <comment>dialog button</comment>
        <translation>تطبيق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="49"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Reset</source>
        <comment>dialog button</comment>
        <translation>إعادة تعيين</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="50"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Restore Defaults</source>
        <comment>dialog button</comment>
        <translation>استعادة الضبط الافتراضي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="51"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Help</source>
        <comment>dialog button</comment>
        <translation>المساعدة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="52"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Save All</source>
        <comment>dialog button</comment>
        <translation>حفظ الكل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="53"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Yes</source>
        <comment>dialog button</comment>
        <translation>نعم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="54"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Yes to All</source>
        <comment>dialog button</comment>
        <translation>نعم للكل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="55"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- No</source>
        <comment>dialog button</comment>
        <translation>لا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="56"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- No to All</source>
        <comment>dialog button</comment>
        <translation>لا للكل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="57"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Abort</source>
        <comment>dialog button</comment>
        <translation>خروج</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="58"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Retry</source>
        <comment>dialog button</comment>
        <translation>إعادة المحاولة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/OverlayDialog.qml" line="59"/>
        <extracomment>dialog button</extracomment>
<source>OverlayDialog -- dialog button --- Ignore</source>
        <comment>dialog button</comment>
        <translation>تجاهل</translation>
    </message>
</context>
<context>
    <name>Page1Welcome</name>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page1Welcome.qml" line="12"/>
        <source>Page1Welcome --- Welcome to</source>
        <translation>مرحبًا بك في</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page1Welcome.qml" line="50"/>
        <source>Page1Welcome --- QUICK TOUR</source>
        <translation>جولة سريعة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page1Welcome.qml" line="80"/>
        <source>Page1Welcome --- LOG IN</source>
        <translation>تسجيل الدخول</translation>
    </message>
</context>
<context>
    <name>Page2Theme</name>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page2Theme.qml" line="21"/>
        <source>Page2Theme --- GETTING STARTED</source>
        <translation>كيف تبدأ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page2Theme.qml" line="22"/>
        <source>Page2Theme --- Choose your theme</source>
        <translation>اختر النُسق المفضل لديك</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page2Theme.qml" line="23"/>
        <source>Page2Theme --- Private Internet Access comes with light and dark themes to fit the look and feel of your desktop.</source>
        <translation>تم تصميم Private Internet Access بنُسق فاتح وآخر داكن ليناسب شكل ومظهر سطح المكتب.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page2Theme.qml" line="47"/>
        <source>Page2Theme --- Dark Theme</source>
        <translation>النُسق الداكن</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page2Theme.qml" line="80"/>
        <source>Page2Theme --- Light Theme</source>
        <translation>النُسق الفاتح</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page2Theme.qml" line="124"/>
        <source>Page2Theme --- Theme preview</source>
        <translation>معاينة النُسق</translation>
    </message>
</context>
<context>
    <name>Page3Customize</name>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page3Customize.qml" line="17"/>
        <source>Page3Customize --- PRO TIP</source>
        <translation>نصيحة احترافية</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page3Customize.qml" line="18"/>
        <source>Page3Customize --- Customize Your VPN Experience</source>
        <translation>أضف طابعك الشخصي على تجربة VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page3Customize.qml" line="19"/>
        <source>Page3Customize --- Choose from a variety of tiles to customize your dashboard to display the information and controls most relevant to you.</source>
        <translation>اختر من بين مجموعة متنوعة من اللوحات لتخصيص لوحة المعلومات لعرض المعلومات وعناصر التحكم الأكثر ملاءمة لك.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page3Customize.qml" line="45"/>
        <source>Page3Customize --- Customization preview</source>
        <translation>معاينة التخصيص</translation>
    </message>
</context>
<context>
    <name>Page4Finish</name>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page4Finish.qml" line="16"/>
        <source>Page4Finish --- GOOD TO GO</source>
        <translation>جاهز للتقدم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page4Finish.qml" line="17"/>
        <source>Page4Finish --- VPN Protection Everywhere You Need It</source>
        <translation>حماية VPN في كل مكان تحتاج إلى حمايته</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page4Finish.qml" line="18"/>
        <source>Page4Finish --- Your Private Internet Access account can protect up to 10 different devices.</source>
        <translation>يمكن لحساب Private Internet Access حماية ما يصل إلى 10 أجهزة مختلفة.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/pages/Page4Finish.qml" line="28"/>
        <source>Page4Finish --- Supported platforms</source>
        <translation>المنصات المدعومة</translation>
    </message>
</context>
<context>
    <name>PageFooter</name>
    <message>
        <location filename="../../../../../client/res/components/onboarding/PageFooter.qml" line="15"/>
        <source>PageFooter --- SKIP TOUR</source>
        <translation>تخطي الجولة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/PageFooter.qml" line="66"/>
        <source>PageFooter --- LOG IN</source>
        <translation>تسجيل الدخول</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/onboarding/PageFooter.qml" line="66"/>
        <source>PageFooter --- NEXT</source>
        <translation>التالي</translation>
    </message>
</context>
<context>
    <name>PerformanceModule</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="20"/>
        <source>PerformanceModule --- Performance tile</source>
        <extracomment>Screen reader annotation for the &quot;Performance&quot; tile containing the performance graph.</extracomment>
        <translation>لوحة الأداء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="74"/>
        <source>PerformanceModule --- PERFORMANCE</source>
        <translation>الأداء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="89"/>
        <source>PerformanceModule --- %1 Mbps</source>
        <translation>%1 Mbps</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="94"/>
        <source>PerformanceModule --- %1 kbps</source>
        <translation>%1 kbps</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="114"/>
        <source>PerformanceModule --- Performance history</source>
        <extracomment>Screen reader annotation for the &quot;Performance&quot; graph, which lists recent performance measurements.</extracomment>
        <translation>سجل الأداء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="248"/>
        <source>PerformanceModule --- 1 minute ago</source>
        <extracomment>Performance graph, 1:00 ago (screen reader annotation)</extracomment>
        <translation>منذ دقيقة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="252"/>
        <source>PerformanceModule --- 2 minutes ago</source>
        <extracomment>Performance graph, 2:00 ago (screen reader annotation)</extracomment>
        <translation>منذ دقيقتين</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="258"/>
        <source>PerformanceModule --- %1 seconds ago</source>
        <extracomment>Performance graph, 0:05-0:55 ago, always a multiple of 5 (screen reader annotation)</extracomment>
        <translation>منذ %1 ثوان</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="262"/>
        <source>PerformanceModule --- 1 minute %1 seconds ago</source>
        <extracomment>Performance graph, 1:05-1:55 ago, always a multiple of 5 (screen reader annotation)</extracomment>
        <translation>منذ دقيقة و%1 ثوان</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="266"/>
        <source>PerformanceModule --- 2 minutes %1 seconds ago</source>
        <extracomment>Performance graph, 2:05-2:55 ago, always a multiple of 5 (screen reader annotation)</extracomment>
        <translation>منذ دقيقتين و%1 ثوان</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="280"/>
        <source>PerformanceModule --- %1 download speed</source>
        <extracomment>Screen reader annotation for performance bar - download speed from a past interval, like &quot;3.5 Mbps download speed&quot;. Speed uses the &quot;%1 Mbps&quot; or &quot;%1 kbps&quot; string from the performance graph</extracomment>
        <translation>سرعة التنزيل %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="360"/>
        <source>PerformanceModule --- Download speed</source>
        <extracomment>Screen reader annotation for the &quot;download&quot; arrow labeling the download speed display</extracomment>
        <translation>سرعة التنزيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="381"/>
        <source>PerformanceModule --- Upload speed</source>
        <extracomment>Screen reader annotation for the &quot;upload&quot; arrow labeling the upload speed display</extracomment>
        <translation>سرعة التحميل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="403"/>
        <source>PerformanceModule --- Connection duration</source>
        <extracomment>Screen reader annotation for the clock icon labeling the connection duration display</extracomment>
        <translation>مدة الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="425"/>
        <source>PerformanceModule -- short-time-part --- 0%1</source>
        <comment>short-time-part</comment>
        <extracomment>Render a time part (hours/minutes/seconds) for the connection duration that&apos;s less than 10. This can pad the value with a leading 0, such as &apos;0%1&apos;.</extracomment>
        <translation>0%1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="429"/>
        <source>PerformanceModule -- long-time-part --- %1</source>
        <comment>long-time-part</comment>
        <extracomment>Render a time part (hours/minutes/seconds) for the connection duration that&apos;s 10 or more.</extracomment>
        <translation>%1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="457"/>
        <source>PerformanceModule -- duration-hours-minutes --- %1:%2</source>
        <comment>duration-hours-minutes</comment>
        <extracomment>Connection duration template for 1 hour or more - %1 is hours, %2 is minutes Hours and minutes are rendered with the time-part or short-time-part strings</extracomment>
        <translation>%1:%2</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/PerformanceModule.qml" line="461"/>
        <source>PerformanceModule -- duration-minutes-seconds --- %1:%2</source>
        <comment>duration-minutes-seconds</comment>
        <extracomment>Connection duration template for less than 1 hour - %1 is minutes, %2 is seconds Minutes and seconds are rendered with the time-part or short-time-part strings</extracomment>
        <translation>%1:%2</translation>
    </message>
</context>
<context>
    <name>PlatformUIStrings</name>
    <message>
        <location filename="../../../../../client/src/platformuistrings.cpp" line="15"/>
        <source>PlatformUIStrings --- Show menu</source>
        <extracomment>Screen reader annotation for the &quot;show menu&quot; action in the Mac tray icon</extracomment>
        <translation>إظهار القائمة</translation>
    </message>
</context>
<context>
    <name>PrivacyPage</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="24"/>
        <source>PrivacyPage --- VPN Killswitch</source>
        <extracomment>Label for the setting that controls the VPN killswitch, a privacy feature that prevents network traffic from leaving the user&apos;s computer unless it is going through the VPN. The term &quot;killswitch&quot; is a recognizable term in the VPN industry that gets used in marketing and can be left unlocalized if there is no clear translation for the concept.</extracomment>
        <translation>مفتاح إنهاء VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="26"/>
        <source>PrivacyPage --- Prevent leaks by blocking traffic outside the VPN</source>
        <extracomment>Descriptive label for the VPN killswitch setting.</extracomment>
        <translation>يمنع التسرب عن طريق منع حركة المرور خارج VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="34"/>
        <source>PrivacyPage --- Off: Don&apos;t block any traffic</source>
        <extracomment>Setting value description for when the VPN killswitch is set to &quot;Off&quot;. No network traffic is blocked and the user&apos;s privacy can potentially be compromised if the VPN connection unexpectedly drops.</extracomment>
        <translation>إيقاف: لا تمنع أي حركة مرور</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="39"/>
        <source>PrivacyPage --- Auto: Block outside traffic when the VPN is on</source>
        <extracomment>Setting value description for when the VPN killswitch is set to &quot;Auto&quot;. Network traffic that tries to go outside the VPN is blocked as long as the user has turned on the VPN, including if the actual VPN connection itself unexpectedly drops.</extracomment>
        <translation>تلقائي: حظر حركة المرور خارج الشبكة عندما تكون VPN قيد التشغيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="44"/>
        <source>PrivacyPage --- Always: Also block all traffic when the VPN is off</source>
        <extracomment>Setting value description for when the VPN killswitch is set to &quot;Always&quot;. Network traffic that tries to go outside the VPN is always blocked, even when the user has switched off the VPN. This effectively disables the user&apos;s internet connection whenever they are not connected to the VPN.</extracomment>
        <translation>دائمًا: حظر كل حركة المرور أيضًا عندما تكون VPN قيد الإيقاف</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="47"/>
        <source>PrivacyPage -- privacy-killswitch --- Off</source>
        <comment>privacy-killswitch</comment>
        <extracomment>These values are used for the Killswitch setting.</extracomment>
        <translation>إيقاف</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="47"/>
        <source>PrivacyPage --- Auto</source>
        <translation>تلقائي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="47"/>
        <source>PrivacyPage --- Always</source>
        <translation>دائمًا</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="64"/>
        <source>PrivacyPage --- PIA MACE requires using PIA DNS in order to function.</source>
        <translation>يتطلب PIA MACE استخدام بروتوكول PIA DNS لكي يعمل.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="65"/>
        <source>PrivacyPage --- PIA MACE</source>
        <translation>PIA MACE</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="66"/>
        <source>PrivacyPage --- Block domains used for ads, trackers, and malware</source>
        <translation>حظر النطاقات المستخدمة للإعلانات والمتعقبات والبرامج الضارة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="72"/>
        <source>PrivacyPage -- privacy-mace --- Off</source>
        <comment>privacy-mace</comment>
        <extracomment>These values are used for the MACE setting.</extracomment>
        <translation>إيقاف</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/PrivacyPage.qml" line="72"/>
        <source>PrivacyPage --- On</source>
        <translation>تشغيل</translation>
    </message>
</context>
<context>
    <name>ProxyPage</name>
    <message>
    <source>ProxyPage --- Redirect the VPN connection through an additional location</source>
        <translation>يعيد توجيه اتصال VPN عبر موقع إضافي</translation>
    </message>
    <message>
    <source>ProxyPage --- Shadowsocks - %1</source>
        <extracomment>Label for the Shadowsocks proxy choice. &quot;Shadowsocks&quot; is a proper noun and shouldn&apos;t be translated, but the dash should match the other proxy choice labels. %1 is a description of the selected region, such as &quot;Japan&quot; or &quot;Auto (US East)&quot;, this uses the localizations defined for the region module.</extracomment>
        <translation>Shadowsocks - %1</translation>
    </message>
    <message>
    <source>ProxyPage --- SOCKS5 Proxy - %1</source>
        <extracomment>Label for the custom SOCKS5 proxy choice when a proxy has been configured. %1 is the configured proxy (host or host:port), such as &quot;SOCKS5 Proxy: 127.0.0.1&quot; or &quot;SOCKS5 Proxy: 172.16.24.18:9080&quot;</extracomment>
        <translation>SOCKS5 Proxy - %1</translation>
    </message>
    <message>
    <source>ProxyPage --- Configure...</source>
        <extracomment>Opens a dialog to specify the custom proxy host/port/credentials.</extracomment>
        <translation>تكوين...</translation>
    </message>
</context>
<context>
    <name>QuickConnectButton</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/QuickConnectButton.qml" line="115"/>
        <source>QuickConnectButton --- Connect to favorite %1</source>
        <extracomment>Screen reader annotation for a Quick Connect button with a heart icon, which is used for a favorite region. %1 is a region name.</extracomment>
        <translation>اتصال بالمفضلة %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/QuickConnectButton.qml" line="120"/>
        <source>QuickConnectButton --- Connect to %1</source>
        <extracomment>Screen reader annotation for a Quick Connect button without a heart icon, which is for a recently-used or nearby location. %1 is a region name.</extracomment>
        <translation>اتصال بـ %1</translation>
    </message>
</context>
<context>
    <name>QuickConnectModule</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/QuickConnectModule.qml" line="16"/>
        <source>QuickConnectModule --- Quick Connect tile</source>
        <extracomment>Screen reader annotation for the Quick Connect tile.</extracomment>
        <translation>لوحة الاتصال السريع</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/QuickConnectModule.qml" line="25"/>
        <source>QuickConnectModule --- QUICK CONNECT</source>
        <translation>اتصال سريع</translation>
    </message>
</context>
<context>
    <name>ReconnectWarning</name>
    <message>
        <location filename="../../../../../client/res/components/settings/ReconnectWarning.qml" line="61"/>
        <source>ReconnectWarning --- Reconnect to apply settings.</source>
        <translation>أعد الاتصال لتطبيق التغييرات.</translation>
    </message>
</context>
<context>
    <name>RegionAuto</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionAuto.qml" line="16"/>
        <source>RegionAuto --- Choose automatically</source>
        <translation>اختر تلقائيًا</translation>
    </message>
</context>
<context>
    <name>RegionDelegate</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionDelegate.qml" line="44"/>
        <source>RegionDelegate --- Port forwarding is not available for this location.</source>
        <translation>إعادة توجيه المنافذ غير متاحة في هذا الموقع.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionDelegate.qml" line="45"/>
        <source>RegionDelegate --- Port forwarding is not available for this country.</source>
        <translation>إعادة توجيه المنافذ غير متوفرة في هذا البلد.</translation>
    </message>
</context>
<context>
    <name>RegionListView</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionListView.qml" line="521"/>
        <source>RegionListView --- Region</source>
        <extracomment>Screen reader annotation for the column in the region list that displays the region names and flags.</extracomment>
        <translation>المنطقة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionListView.qml" line="527"/>
        <source>RegionListView --- Latency</source>
        <extracomment>Screen reader annotation for the column in the region list that displays the regions&apos; latency measurements.</extracomment>
        <translation>زمن الانتقال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionListView.qml" line="533"/>
        <source>RegionListView --- Favorite</source>
        <extracomment>Screen reader annotation for the column in the region list that displays the regions&apos; &quot;favorite region&quot; buttons.</extracomment>
        <translation>تفضيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionListView.qml" line="73"/>
        <source>RegionListView --- Region list</source>
        <extracomment>Screen reader annotation for the region list on the regions page, where users can choose a region and mark regions as favorites. (Also used to describe the scroll bar for the region list.)</extracomment>
        <translation>قائمة المناطق</translation>
    </message>
</context>
<context>
    <name>RegionModule</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/RegionModule.qml" line="74"/>
        <source>RegionModule --- Auto</source>
        <translation>تلقائي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/RegionModule.qml" line="72"/>
        <source>RegionModule --- Auto (%1)</source>
        <translation>تلقائي (%1)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/RegionModule.qml" line="19"/>
        <source>RegionModule --- Region tile</source>
        <extracomment>Screen reader annotation for the Region tile, which users can click to go to the Region page and choose a region.</extracomment>
        <translation>لوحة المنطقة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/RegionModule.qml" line="49"/>
        <source>RegionModule --- VPN SERVER</source>
        <translation>خادم VPN</translation>
    </message>
    <message>
    <source>RegionModule --- VIA SOCKS5 PROXY</source>
        <translation>عبر وكيل SOCKS5</translation>
    </message>
    <message>
    <source>RegionModule --- VIA SHADOWSOCKS</source>
        <translation>عبر SHADOWSOCKS</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/RegionModule.qml" line="102"/>
        <source>RegionModule --- Select region</source>
        <extracomment>Screen reader annotation for the button that makes up the Region tile, which users can click to go to the Region page and select a region. Should be a short description of the &quot;select region&quot; action.</extracomment>
        <translation>تحديد المنطقة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/RegionModule.qml" line="107"/>
        <source>RegionModule --- Select region, %1 is selected</source>
        <extracomment>Screen reader description for the Region tile button. Should begin with the &quot;Select region&quot; translation, since that&apos;s what the button does. This also includes the currently-selected region, which the button displays. %1 is a region name.</extracomment>
        <translation>تحديد المنطقة، تم تحديد %1</translation>
    </message>
</context>
<context>
    <name>RegionPage</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionPage.qml" line="48"/>
        <source>RegionPage --- Search...</source>
        <translation>بحث...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionPage.qml" line="73"/>
        <source>RegionPage --- Name</source>
        <translation>الاسم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionPage.qml" line="77"/>
        <source>RegionPage --- Latency</source>
        <translation>زمن الانتقال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionPage.qml" line="17"/>
        <source>RegionPage --- Back to Connect page</source>
        <extracomment>Screen reader description of the &quot;Back&quot; button in the header when the user is on the Region page. This is a slightly longer description of the button&apos;s action, which in this case returns to the Connect page.</extracomment>
        <translation>عودة إلى صفحة الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionPage.qml" line="66"/>
        <source>RegionPage --- Region list sort</source>
        <extracomment>Screen reader annotation for the &quot;Name / Latency&quot; heading above the region list, which sorts by either name or latency. The screen reader will indicate that this is a group of controls.</extracomment>
        <translation>فرز قائمة المناطق</translation>
    </message>
</context>
<context>
    <name>RegionRowBase</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionRowBase.qml" line="130"/>
        <source>RegionRowBase --- %1, does not support port forwarding</source>
        <extracomment>Screen reader annotation used for a region that does not support port forwarding when the feature is enabled. Corresponds to the &quot;slashed-arrow&quot; indicator and &quot;Port forwarding is not supported by this region/country.&quot; tips. %1 is a translated region name. The region name should come first so the screen reader reads it first.</extracomment>
        <translation>%1، لا تدعم إعادة توجيه المنافذ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionRowBase.qml" line="149"/>
        <source>RegionRowBase --- Favorite region</source>
        <extracomment>Screen reader annotation for the &quot;favorite&quot; button (heart icon) next to regions in the regions list. (The screen reader will indicate whether the button is &quot;on&quot; or &quot;off&quot;.)</extracomment>
        <translation>تفضيل المنطقة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/region/RegionRowBase.qml" line="271"/>
        <source>RegionRowBase --- %1 ms</source>
        <translation>%1 مل ث</translation>
    </message>
</context>
<context>
    <name>SettingsMessages</name>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsMessages.qml" line="15"/>
        <source>SettingsMessages --- Restart your computer to finish installing the split tunnel filter.</source>
        <translation>أعد تشغيل الكمبيوتر لإكمال تثبيت مرشح النفق المقسم.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsMessages.qml" line="17"/>
        <source>SettingsMessages --- The split tunnel filter could not be installed.  Try restarting your computer, or contact support if this problem persists.</source>
        <translation>لا يمكن تثبيت مرشح النفق المقسم. جرّب إعادة تشغيل الكمبيوتر، أو اتصل بالدعم إذا استمرت هذه المشكلة.</translation>
    </message>
    <message>
    <source>SettingsMessages --- This feature requires macOS 10.13 or later.</source>
        <translation>تتطلب هذه الميزة macOS 10.13 أو أحدث.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsMessages.qml" line="20"/>
        <source>SettingsMessages --- This feature requires Windows 7 Service Pack 1 or later.</source>
        <translation>تتطلب هذه الميزة Windows 7 Service Pack 1 أو إصدار أحدث.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsMessages.qml" line="21"/>
        <source>SettingsMessages --- This feature requires iptables 1.6.1 or later.</source>
        <translation>تتطلب هذه الميزة iptables 1.6.1 أو أحدث.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsMessages.qml" line="25"/>
        <source>SettingsMessages --- This feature requires net_cls to be available at %1.</source>
        <extracomment>Message for Linux indicating that a kernel feature has to be mounted at a specific location to use the split tunnel feature. %1 is a file path, currently /sys/fs/cgroups/net_cls.</extracomment>
        <translation>تتطلب هذه الميزة توفر net_cls على %1.</translation>
    </message>
</context>
<context>
    <name>SettingsModule</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="17"/>
        <source>SettingsModule --- Quick Settings tile</source>
        <extracomment>Screen reader annotation for the Quick Settings tile.</extracomment>
        <translation>لوحة الإعدادات السريعة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="25"/>
        <source>SettingsModule --- QUICK SETTINGS</source>
        <translation>إعدادات سريعة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="55"/>
        <source>SettingsModule --- Desktop Notifications</source>
        <translation>إشعارات سطح المكتب</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="61"/>
        <source>SettingsModule --- MACE</source>
        <translation>MACE</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="68"/>
        <source>SettingsModule --- Port Forwarding</source>
        <translation>إعادة توجيه المنافذ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="74"/>
        <source>SettingsModule --- Allow LAN</source>
        <translation>السماح لشبكة LAN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="81"/>
        <source>SettingsModule --- Debug Logging</source>
        <translation>تسجيل تصحيح الأخطاء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="99"/>
        <source>SettingsModule --- Light Theme</source>
        <translation>النُسق الفاتح</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/settings/SettingsModule.qml" line="110"/>
        <source>SettingsModule --- View All Settings...</source>
        <translation>عرض جميع الإعدادات...</translation>
    </message>
</context>
<context>
    <name>SettingsWindow</name>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="50"/>
        <extracomment>setting-title</extracomment>
<source>SettingsWindow -- setting-title --- General</source>
        <comment>setting-title</comment>
        <translation>عام</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="51"/>
        <extracomment>setting-title</extracomment>
<source>SettingsWindow -- setting-title --- Account</source>
        <comment>setting-title</comment>
        <translation>الحساب</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="52"/>
        <extracomment>setting-title</extracomment>
<source>SettingsWindow -- setting-title --- Privacy</source>
        <comment>setting-title</comment>
        <translation>الخصوصية</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="53"/>
        <extracomment>setting-title</extracomment>
<source>SettingsWindow -- setting-title --- Network</source>
        <comment>setting-title</comment>
        <translation>الشبكة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="54"/>
        <extracomment>setting-title</extracomment>
<source>SettingsWindow -- setting-title --- Connection</source>
        <comment>setting-title</comment>
        <translation>الاتصال</translation>
    </message>
    <message>
    <source>SettingsWindow -- setting-title --- Proxy</source>
        <extracomment>setting-title</extracomment>
        <translation>الوكيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="55"/>
        <extracomment>setting-title</extracomment>
<source>SettingsWindow -- setting-title --- Help</source>
        <comment>setting-title</comment>
        <translation>المساعدة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="58"/>
        <extracomment>setting-heading</extracomment>
<source>SettingsWindow -- setting-heading --- General Preferences</source>
        <comment>setting-heading</comment>
        <translation>تفضيلات عامة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="59"/>
        <extracomment>setting-heading</extracomment>
<source>SettingsWindow -- setting-heading --- Account Information</source>
        <comment>setting-heading</comment>
        <translation>معلومات الحساب</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="60"/>
        <extracomment>setting-heading</extracomment>
<source>SettingsWindow -- setting-heading --- Privacy Preferences</source>
        <comment>setting-heading</comment>
        <translation>تفضيلات الخصوصية</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="61"/>
        <extracomment>setting-heading</extracomment>
<source>SettingsWindow -- setting-heading --- Network Preferences</source>
        <comment>setting-heading</comment>
        <translation>تفضيلات الشبكة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="62"/>
        <extracomment>setting-heading</extracomment>
<source>SettingsWindow -- setting-heading --- Connection Preferences</source>
        <comment>setting-heading</comment>
        <translation>تفضيلات الاتصال</translation>
    </message>
    <message>
    <source>SettingsWindow -- setting-heading --- Proxy Preferences</source>
        <extracomment>setting-heading</extracomment>
        <translation>تفضيلات الوكيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="63"/>
        <extracomment>setting-heading</extracomment>
<source>SettingsWindow -- setting-heading --- Help</source>
        <comment>setting-heading</comment>
        <translation>المساعدة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="124"/>
        <source>SettingsWindow --- Alert</source>
        <translation>تنبيه</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/SettingsWindow.qml" line="158"/>
        <source>SettingsWindow --- Settings</source>
        <translation>الإعدادات</translation>
    </message>
</context>
<context>
    <name>SnoozeModule</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="18"/>
        <source>SnoozeModule --- VPN Snooze tile</source>
        <extracomment>Screen reader annotation for the Snooze tile</extracomment>
        <translation>لوح غفوة VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="35"/>
        <source>SnoozeModule --- RESUMING CONNECTION</source>
        <translation>جار استئناف الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="37"/>
        <source>SnoozeModule --- SNOOZED</source>
        <translation>في غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="39"/>
        <source>SnoozeModule --- SNOOZING</source>
        <translation>دخول غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="42"/>
        <source>SnoozeModule --- VPN SNOOZE</source>
        <translation>غفوة VPN</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="182"/>
        <source>SnoozeModule --- Decrease snooze time</source>
        <extracomment>Screen reader name for the &quot;minus&quot; button that decreases snooze time</extracomment>
        <translation>تقليل مدة الغفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="205"/>
        <source>SnoozeModule --- Snooze time</source>
        <extracomment>Screen reader annotation for the snooze time display in the Snooze tile</extracomment>
        <translation>مدة الغفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="254"/>
        <source>SnoozeModule --- Increase snooze time</source>
        <extracomment>Screen reader name for the &quot;plus&quot; button that increases snooze time</extracomment>
        <translation>زيادة مدة الغفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="304"/>
        <source>SnoozeModule --- Snooze</source>
        <translation>غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="311"/>
        <source>SnoozeModule --- Resume</source>
        <translation>استئناف</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/SnoozeModule.qml" line="332"/>
        <source>SnoozeModule --- Snooze temporarily disconnects the VPN and automatically reconnects when the timer elapses.</source>
        <translation>الغفوة تؤدي إلى قطع اتصال VPN مؤقتًا وإعادة الاتصال تلقائيًا عندما تنتهي مدة المؤقت.</translation>
    </message>
</context>
<context>
    <name>SplashContent</name>
    <message>
        <location filename="../../../../../client/res/components/splash/SplashContent.qml" line="91"/>
        <source>SplashContent --- Send Logs</source>
        <translation>إرسال السجلات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/splash/SplashContent.qml" line="112"/>
        <source>SplashContent --- Reinstall</source>
        <translation>إعادة التثبيت</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/splash/SplashContent.qml" line="135"/>
        <source>SplashContent --- Quit</source>
        <translation>خروج</translation>
    </message>
</context>
<context>
    <name>SplashPopup</name>
    <message>
        <location filename="../../../../../client/res/components/splash/SplashPopup.qml" line="40"/>
        <source>SplashPopup --- Quit</source>
        <translation>خروج</translation>
    </message>
</context>
<context>
    <name>SplitTunnelAddAppRow</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAddAppRow.qml" line="59"/>
        <source>SplitTunnelAddAppRow --- Add Application</source>
        <translation>إضافة تطبيق</translation>
    </message>
</context>
<context>
    <name>SplitTunnelAppDialog</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="75"/>
        <source>SplitTunnelAppDialog --- Only executable files can be excluded from VPN. Please select an executable program or shell script.</source>
        <translation>يمكن استبعاد الملفات التنفيذية فقط من VPN. يرجى تحديد برنامج تنفيذي أو شيل سكربت.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="75"/>
        <source>SplitTunnelAppDialog --- Unable to exclude application</source>
        <translation>تعذّر استبعاد التطبيق</translation>
    </message>
    <message>
    <source>SplitTunnelAppDialog --- Add</source>
        <translation>إضافة</translation>
    </message>
    <message>
    <source>SplitTunnelAppDialog --- Exclude</source>
        <translation>استبعاد</translation>
    </message>
    <message>
    <source>SplitTunnelAppDialog --- Cancel</source>
        <translation>إلغاء</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="137"/>
        <source>SplitTunnelAppDialog --- Add Application</source>
        <translation>إضافة تطبيق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="180"/>
        <source>SplitTunnelAppDialog --- Search</source>
        <translation>بحث</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="193"/>
        <source>SplitTunnelAppDialog --- Browse</source>
        <translation>استعراض</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="245"/>
        <source>SplitTunnelAppDialog --- Applications</source>
        <translation>التطبيقات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="461"/>
        <source>SplitTunnelAppDialog --- App list, loading</source>
        <extracomment>Screen reader annotation for the &quot;application list&quot; from which a user can select an application to exclude. Screen reader annotation used for the split tunnel app list when the apps are still being loaded.</extracomment>
        <translation>قائمة التطبيقات، قيد التحميل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="464"/>
        <source>SplitTunnelAppDialog --- App list</source>
        <extracomment>Screen reader annotation for the split tunnel app list.</extracomment>
        <translation>قائمة التطبيقات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="470"/>
        <source>SplitTunnelAppDialog --- App</source>
        <extracomment>Screen reader annotation for the column listing the application in the existing application list (this table has only one column)</extracomment>
        <translation>التطبيق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppDialog.qml" line="496"/>
        <source>SplitTunnelAppDialog --- Refresh</source>
        <translation>تحديث</translation>
    </message>
</context>
<context>
    <name>SplitTunnelAppRow</name>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppRow.qml" line="120"/>
        <source>SplitTunnelAppRow --- Remove</source>
        <extracomment>Screen reader annotation for the &quot;remove&quot; button (&quot;X&quot; icon) next to a split tunnel app rule. (Should be labeled like a normal command button.)</extracomment>
        <translation>إزالة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/settings/pages/SplitTunnelAppRow.qml" line="157"/>
        <source>SplitTunnelAppRow --- Microsoft Store app</source>
        <translation>تطبيق Microsoft Store</translation>
    </message>
</context>
<context>
    <name>TabLayoutCommon</name>
    <message>
        <location filename="../../../../../client/res/components/settings/tabs/TabLayoutCommon.qml" line="8"/>
        <source>TabLayoutCommon --- Settings tabs</source>
        <extracomment>Screen reader annotation for the group of tabs in the Settings window.</extracomment>
        <translation>تبويبات الإعدادات</translation>
    </message>
</context>
<context>
    <name>ThemedTextField</name>
    <message>
        <location filename="../../../../../client/res/components/common/ThemedTextField.qml" line="74"/>
        <source>ThemedTextField --- Undo</source>
        <translation>تراجع</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ThemedTextField.qml" line="82"/>
        <source>ThemedTextField --- Redo</source>
        <translation>إعادة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ThemedTextField.qml" line="93"/>
        <source>ThemedTextField --- Cut</source>
        <translation>قص</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ThemedTextField.qml" line="101"/>
        <source>ThemedTextField --- Copy</source>
        <translation>نسخ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ThemedTextField.qml" line="109"/>
        <source>ThemedTextField --- Paste</source>
        <translation>لصق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ThemedTextField.qml" line="117"/>
        <source>ThemedTextField --- Delete</source>
        <translation>حذف</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/ThemedTextField.qml" line="126"/>
        <source>ThemedTextField --- Select All</source>
        <translation>تحديد الكل</translation>
    </message>
</context>
<context>
    <name>TrayIcon</name>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="46"/>
        <source>TrayIcon --- Private Internet Access</source>
        <translation>Private Internet Access</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="50"/>
        <source>TrayIcon --- Resuming Connection...</source>
        <translation>جار استئناف الاتصال...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="52"/>
        <source>TrayIcon --- Snoozing...</source>
        <translation>دخول غفوة...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="54"/>
        <source>TrayIcon --- Snoozed</source>
        <translation>في غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="60"/>
        <source>TrayIcon --- Connected to %1</source>
        <translation>تم الاتصال بـ %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="63"/>
        <source>TrayIcon --- Reconnecting...</source>
        <translation>جارٍ إعادة الاتصال...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="65"/>
        <source>TrayIcon --- Connecting...</source>
        <translation>جارٍ الاتصال...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="67"/>
        <source>TrayIcon --- Disconnecting...</source>
        <translation>جارٍ قطع الاتصال...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="72"/>
        <source>TrayIcon --- Killswitch Active</source>
        <translation>مفتاح الإنهاء نشط</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayIcon.qml" line="74"/>
        <source>TrayIcon --- Update Available</source>
        <translation>هناك تحديث متاح</translation>
    </message>
</context>
<context>
    <name>TrayManager</name>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="61"/>
        <source>TrayManager --- Snoozing</source>
        <translation>دخول غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="64"/>
        <source>TrayManager --- Resuming Connection</source>
        <translation>جار استئناف الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="67"/>
        <source>TrayManager --- Snoozed</source>
        <translation>في غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="84"/>
        <source>TrayManager --- Internet access has been disabled by the killswitch.</source>
        <translation>تم تعطيل الوصول إلى الإنترنت بواسطة مفتاح الإنهاء.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="87"/>
        <source>TrayManager --- Disconnected from %1</source>
        <translation>تم قطع الاتصال بـ %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="88"/>
        <source>TrayManager --- Disconnected</source>
        <translation>غير متصل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="91"/>
        <source>TrayManager --- Connected to %1</source>
        <translation>تم الاتصال بـ %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="106"/>
        <source>TrayManager --- Forwarded port %1</source>
        <translation>المنفذ المعاد توجيهه %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="119"/>
        <source>TrayManager --- Port forward request failed</source>
        <translation>تعذَّر طلب إعادة توجيه المنفذ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="138"/>
        <source>TrayManager --- Reconnecting to %1...</source>
        <translation>جارٍ إعادة الاتصال بـ %1...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="140"/>
        <source>TrayManager --- Connecting to %1...</source>
        <translation>جارٍ الاتصال بـ %1...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="144"/>
        <source>TrayManager --- Disconnecting from %1...</source>
        <translation>جارٍ قطع الاتصال بـ %1...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayManager.qml" line="145"/>
        <source>TrayManager --- Disconnecting...</source>
        <translation>جارٍ قطع الاتصال...</translation>
    </message>
</context>
<context>
    <name>TrayMenuBuilder</name>
    <message>
        <location filename="../../../../../client/res/components/common/Messages.qml" line="16"/>
        <source>TrayMenuBuilder --- Help</source>
        <extracomment>Help label used for the Help tray menu as well as help links on some settings</extracomment>
        <translation>المساعدة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="47"/>
        <source>TrayMenuBuilder --- Show Window</source>
        <extracomment>Menu command to display the main app window/dashboard.</extracomment>
        <translation>إظهار النافذة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="54"/>
        <source>TrayMenuBuilder --- Resume Connection</source>
        <translation>استئناف الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="57"/>
        <source>TrayMenuBuilder --- Connect (Auto)</source>
        <extracomment>Menu command to connect to an automatically chosen region.</extracomment>
        <translation>اتصال (تلقائي)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="59"/>
        <source>TrayMenuBuilder --- Connect (%1)</source>
        <extracomment>Menu command to connect to a specific bookmarked region, with the region name in parentheses.</extracomment>
        <translation>اتصال (%1)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="71"/>
        <source>TrayMenuBuilder --- Disconnect</source>
        <extracomment>Menu command to disconnect from the VPN.</extracomment>
        <translation>قطع الاتصال</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="74"/>
        <source>TrayMenuBuilder --- Snooze</source>
        <translation>غفوة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="75"/>
        <source>TrayMenuBuilder --- 5 Minutes</source>
        <translation>5 دقائق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="76"/>
        <source>TrayMenuBuilder --- 10 Minutes</source>
        <translation>10 دقائق</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="77"/>
        <source>TrayMenuBuilder --- 15 Minutes</source>
        <translation>15 دقيقة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="78"/>
        <source>TrayMenuBuilder --- 30 Minutes</source>
        <translation>30 دقيقة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="123"/>
        <source>TrayMenuBuilder --- Connect to</source>
        <extracomment>Menu label for a submenu containing a list of regions to connect to.</extracomment>
        <translation>اتصال بـ</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="129"/>
        <source>TrayMenuBuilder --- Log In</source>
        <extracomment>Menu command to display the main app window where the user can log in.</extracomment>
        <translation>تسجيل الدخول</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="140"/>
        <source>TrayMenuBuilder --- Settings...</source>
        <extracomment>Menu command to open the settings dialog.</extracomment>
        <translation>الإعدادات...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="144"/>
        <source>TrayMenuBuilder --- Enable Debug Logging</source>
        <extracomment>Menu label for a submenu containing help and support items. Menu command to enable or disable debug logging, which stores additional information that help developers identify and debug problems.</extracomment>
        <translation>تمكين تسجيل التصحيح</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="146"/>
        <source>TrayMenuBuilder --- Submit Debug Logs...</source>
        <extracomment>Menu command to open a dialog that lets the user submit collected debug logs to developers for debugging.</extracomment>
        <translation>إرسال سجلات التصحيح...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="149"/>
        <source>TrayMenuBuilder --- Changelog</source>
        <extracomment>Menu command to display a list of changes introduced in each version of the application.</extracomment>
        <translation>سجل التغييرات</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="152"/>
        <source>TrayMenuBuilder --- Support Portal</source>
        <extracomment>Menu command to open the support portal website in the user&apos;s browser.</extracomment>
        <translation>بوابة الدعم</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="154"/>
        <source>TrayMenuBuilder --- Blog</source>
        <extracomment>Menu command to open the company blog website in the user&apos;s browser.</extracomment>
        <translation>المدونة</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="162"/>
        <source>TrayMenuBuilder --- Copy Public IP (%1)</source>
        <extracomment>Menu command to copy the user&apos;s current public IP address to the clipboard. The %1 placeholder contains the IP address, e.g. &quot;10.0.23.45&quot;.</extracomment>
        <translation>نسخ عنوان IP العمومي (%1)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="168"/>
        <source>TrayMenuBuilder --- Copy Public IP</source>
        <extracomment>Menu command to copy the user&apos;s current public IP address to the clipboard. This variation should match the &quot;Copy Public IP (%1)&quot; string, but omits the parenthesis and is shown grayed out, used when disconnected.</extracomment>
        <translation>نسخ عنوان IP العمومي</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="176"/>
        <source>TrayMenuBuilder --- Copy Forwarded Port (%1)</source>
        <extracomment>Menu command to copy the port number that is currently being forwarded (from the VPN to the user&apos;s computer) to the clipboard. The %1 placeholder contains the port number, e.g. &quot;47650&quot;.</extracomment>
        <translation>نسخ المنفذ المعاد توجيهه (%1)</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="184"/>
        <source>TrayMenuBuilder --- Copy Forwarded Port</source>
        <extracomment>Menu command to copy the port number that is currently being forwarded (from the VPN to the user&apos;s computer) to the clipboard. This variation should match the &quot;Copy Forwarded Port (%1)&quot; string, but omits the parenthesis and is shown grayed out, used when port forwarding is not available.</extracomment>
        <translation>نسخ المنفذ المعاد توجيهه</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/TrayMenuBuilder.qml" line="190"/>
        <source>TrayMenuBuilder --- Quit</source>
        <extracomment>Menu command to quit the application.</extracomment>
        <translation>خروج</translation>
    </message>
</context>
<context>
    <name>UpdateNotificationStatus</name>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="75"/>
        <source>UpdateNotificationStatus --- Download of v%1 failed</source>
        <translation>تعذَّر تنزيل الإصدار %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="80"/>
        <source>UpdateNotificationStatus --- Ready to install v%1</source>
        <translation>جاهز لتثبيت الإصدار %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="109"/>
        <source>UpdateNotificationStatus --- Download v%1</source>
        <translation>تنزيل الإصدار %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="111"/>
        <source>UpdateNotificationStatus --- (%2%) Install v%1</source>
        <translation>(%2%) تثبيت الإصدار %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="114"/>
        <source>UpdateNotificationStatus --- Install v%1</source>
        <translation>تثبيت الإصدار %1</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="142"/>
        <source>UpdateNotificationStatus --- Version %1 is available.</source>
        <translation>الإصدار %1 متوفر.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="144"/>
        <source>UpdateNotificationStatus --- Downloading v%1...</source>
        <translation>جارٍ تنزيل الإصدار %1...</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="146"/>
        <source>UpdateNotificationStatus --- Ready to install v%1.</source>
        <translation>جاهز لتثبيت الإصدار %1.</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="158"/>
        <source>UpdateNotificationStatus --- Download</source>
        <translation>تنزيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="165"/>
        <source>UpdateNotificationStatus --- Install</source>
        <translation>تثبيت</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/common/UpdateNotificationStatus.qml" line="219"/>
        <source>UpdateNotificationStatus --- Version %1 is available</source>
        <translation>الإصدار %1 متوفر</translation>
    </message>
</context>
<context>
    <name>UsageModule</name>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/UsageModule.qml" line="14"/>
        <source>UsageModule --- Usage tile</source>
        <extracomment>Screen reader annotation for the Usage tile.</extracomment>
        <translation>لوحة الاستخدام</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/UsageModule.qml" line="19"/>
        <source>UsageModule --- %1 MB</source>
        <translation>%1 ميغابايت</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/UsageModule.qml" line="23"/>
        <source>UsageModule --- USAGE</source>
        <translation>الاستخدام</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/UsageModule.qml" line="34"/>
        <source>UsageModule --- Download</source>
        <translation>تنزيل</translation>
    </message>
    <message>
        <location filename="../../../../../client/res/components/dashboard/connect/modules/UsageModule.qml" line="56"/>
        <source>UsageModule --- Upload</source>
        <translation>تحميل</translation>
    </message>
</context>
<context>
    <name>winstaller</name>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="37"/>
        <source>winstaller --- Private Internet Access Installer</source>
        <translation>مثبت Private Internet Access</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="38"/>
        <source>winstaller --- %.0f SECONDS REMAINING</source>
        <translation>متبقي %.0f ثانية</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="40"/>
        <source>winstaller --- INSTALL</source>
        <translation>تثبيت</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="41"/>
        <source>winstaller --- UNINSTALL</source>
        <translation>إزالة التثبيت</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="42"/>
        <source>winstaller --- FINISH</source>
        <translation>إنهاء</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="44"/>
        <source>winstaller --- PREPARING</source>
        <translation>جارٍ الإعداد</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="45"/>
        <source>winstaller --- READY TO INSTALL</source>
        <translation>جاهز للتثبيت</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="46"/>
        <source>winstaller --- INSTALLING</source>
        <translation>جارٍ التثبيت</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="47"/>
        <source>winstaller --- ABORTING</source>
        <translation>جارٍ الإيقاف</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="48"/>
        <source>winstaller --- FINISHING UP</source>
        <translation>جارٍ الإنهاء</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="49"/>
        <source>winstaller --- CLEANING UP</source>
        <translation>جارٍ التنظيف</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="50"/>
        <source>winstaller --- ROLLING BACK CHANGES</source>
        <translation>جارٍ التراجع عن التغييرات</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="51"/>
        <source>winstaller --- FINISHED</source>
        <translation>انتهى</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="52"/>
        <source>winstaller --- ABORTED</source>
        <translation>تم الإيقاف</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="53"/>
        <source>winstaller --- UNPACKING</source>
        <translation>جارٍ فك الضغط</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="54"/>
        <source>winstaller --- COPYING FILES</source>
        <translation>جارٍ نسخ الملفات</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="55"/>
        <source>winstaller --- SHUTTING DOWN CLIENT</source>
        <translation>جارٍ إغلاق العميل</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="56"/>
        <source>winstaller --- STOPPING SERVICE</source>
        <translation>جارٍ إيقاف الخدمة</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="57"/>
        <source>winstaller --- UNREGISTERING SERVICE</source>
        <translation>جارٍ إلغاء تسجيل الخدمة</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="58"/>
        <source>winstaller --- REGISTERING SERVICE</source>
        <translation>جارٍ تسجيل الخدمة</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="59"/>
        <source>winstaller --- STARTING SERVICE</source>
        <translation>جارٍ بدء الخدمة</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="60"/>
        <source>winstaller --- REMOVING NETWORK ADAPTER</source>
        <translation>جارٍ إزالة بطاقة الشبكة</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="61"/>
        <source>winstaller --- INSTALLING NETWORK ADAPTER</source>
        <translation>جارٍ تثبيت بطاقة الشبكة</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="62"/>
        <source>winstaller --- BACKING UP FILES</source>
        <translation>جارٍ النسخ الاحتياطي للملفات</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="63"/>
        <source>winstaller --- REMOVING PREVIOUS VERSION</source>
        <translation>جارٍ إزالة الإصدار السابق</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="64"/>
        <source>winstaller --- REMOVING FILES</source>
        <translation>جارٍ إزالة الملفات</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="65"/>
        <source>winstaller --- REMOVING CALLOUT DRIVER</source>
        <translation>إزالة برنامج تشغيل الاستدعاء</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="66"/>
        <source>winstaller --- UPDATING CALLOUT DRIVER</source>
        <translation>تحديث برنامج تشغيل الاستدعاء</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="68"/>
        <source>winstaller --- Unable to recognize the existing installation. Would you like to delete the entire directory?</source>
        <translation>تعذّر التعرف على التثبيت الموجود. هل ترغب في حذف الدليل بالكامل؟</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="69"/>
        <source>winstaller --- Corrupt installation detected</source>
        <translation>تم الكشف عن تثبيت معطوب</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="70"/>
        <source>winstaller --- There are stray files in the installation directory. Would you like to delete them?</source>
        <translation>هناك ملفات ضائعة في دليل التثبيت. هل ترغب في حذفها؟</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="71"/>
        <source>winstaller --- Files remaining</source>
        <translation>الملفات المتبقية</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="72"/>
        <source>winstaller --- Do you want to abort the installation?</source>
        <translation>هل تريد إيقاف التثبيت؟</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="73"/>
        <source>winstaller --- Abort installation?</source>
        <translation>إيقاف التثبيت؟</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="74"/>
        <source>winstaller --- Error</source>
        <translation>خطأ</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="75"/>
        <source>winstaller --- This will replace your existing installation of Private Internet Access. Downgrading afterwards will require a clean reinstall. Do you wish to continue?</source>
        <translation>سيحل هذا محل التثبيت الحالي لتطبيقPrivate Internet Access. سوف يتطلب الرجوع لإصدار أقدم بعد ذلك إعادة تثبيت نظيفة. هل ترغب في المتابعة؟</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="76"/>
        <source>winstaller --- Replace existing version?</source>
        <translation>هل تريد استبدال الإصدار الحالي؟</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="77"/>
        <source>winstaller --- Unable to remove directory: %s</source>
        <translation>تعذّرت إزالة الدليل: %s</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="78"/>
        <source>winstaller --- Please exit the existing Private Internet Access before proceeding.</source>
        <translation>يرجى الخروج من Private Internet Access الحالية قبل المتابعة.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="79"/>
        <source>winstaller --- There was a problem uninstalling the existing version.</source>
        <translation>حدثت مشكلة في إزالة تثبيت الإصدار الحالي.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="80"/>
        <source>winstaller --- There are still clients running.</source>
        <translation>لا يزال هناك عملاء قيد التشغيل.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="81"/>
        <source>winstaller --- Failed to stop the service.</source>
        <translation>تعذَّر إيقاف الخدمة.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="82"/>
        <source>winstaller --- A reboot is needed before installation can proceed.</source>
        <translation>يجب إعادة تشغيل النظام قبل إتمام عملية التثبيت.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="83"/>
        <source>winstaller --- Service executable missing.</source>
        <translation>هناك قابلة للتنفيذ مفقودة.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="84"/>
        <source>winstaller --- Service uninstallation failed.</source>
        <translation>تعذَّرت إزالة التثبيت.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="85"/>
        <source>winstaller --- The service installation failed.</source>
        <translation>تعذَّر تثبيت الخدمة.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="86"/>
        <source>winstaller --- The service failed to start.</source>
        <translation>تعذَّر بدء الخدمة.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="87"/>
        <source>winstaller --- Error uninstalling TAP driver.</source>
        <translation>خطأ في إلغاء تثبيت برنامج تشغيل TAP.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="88"/>
        <source>winstaller --- The TAP driver was not approved for installation.</source>
        <translation>لم يتم اعتماد تثبيت برنامج تشغيل TAP.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="89"/>
        <source>winstaller --- The TAP driver installation failed.</source>
        <translation>تعذَّر تثبيت برنامج تشغيل TAP.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="90"/>
        <source>winstaller --- Private Internet Access requires at least Windows 7 to run.</source>
        <translation>يتطلب Private Internet Access نظام Windows 7 على الأقل لكي يعمل.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="91"/>
        <source>winstaller --- The 32-bit version of Private Internet Access can only run on a 32-bit version of Windows.</source>
        <translation>يمكن تشغيل إصدار 32 بت من Private Internet Access على إصدار 32 بت فقط من نظام Windows.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="92"/>
        <source>winstaller --- Failed to retrieve shell folder %s.</source>
        <translation>تعذَّر استرداد مجلد Shell %s.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="93"/>
        <source>winstaller --- Out of memory.</source>
        <translation>نفذت الذاكرة.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="94"/>
        <source>winstaller --- Payload decompression error.</source>
        <translation>خطأ في فك ضغط البيانات الأساسية.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="95"/>
        <source>winstaller --- Corrupted payload (CRC error).</source>
        <translation>بيانات أساسية معطوبة (خطأ CRC).</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="96"/>
        <source>winstaller --- Unable to create file: %s</source>
        <translation>تعذّر إنشاء الملف: %s</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="97"/>
        <source>winstaller --- Unable to write file: %s</source>
        <translation>تعذّرت كتابة الملف: %s</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="98"/>
        <source>winstaller --- Unable to write entire file: %s</source>
        <translation>تعذّرت كتابة الملف الكامل: %s</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="99"/>
        <source>winstaller --- Unable to initialize decompressor.</source>
        <translation>تعذّرت تهيئة برنامج فك الضغط.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="100"/>
        <source>winstaller --- Corrupted payload (invalid parameters).</source>
        <translation>بيانات أساسية معطوبة (معلمات غير صالحة).</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="101"/>
        <source>winstaller --- Unable to move %s</source>
        <translation>تعذّر نقل %s</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="102"/>
        <source>winstaller --- Unable to create rollback directory. If you proceed, the installation can&apos;t undo any changes if there&apos;s an error.</source>
        <translation>تعذّر إنشاء دليل التراجع. في حالة المتابعة، لا يمكن للتثبيت التراجع عن أي تغييرات إذا حدث خطأ.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="103"/>
        <source>winstaller --- Unable to create directory: %s</source>
        <translation>تعذّر إنشاء الدليل: %s</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="104"/>
        <source>winstaller --- The installer does not contain a recognizable payload.</source>
        <translation>لا يحتوي المثبت على بيانات أساسية يمكن التعرف عليها.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="105"/>
        <source>winstaller --- Missing payload</source>
        <translation>البيانات الأساسية ناقصة</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="106"/>
        <source>winstaller --- Error uninstalling callout driver.</source>
        <translation>خطأ في إزالة تثبيت برنامج تشغيل الاستدعاء.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="107"/>
        <source>winstaller --- Callout driver installation failed.</source>
        <translation>تعذّر تثبيت برنامج تشغيل الاستدعاء.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="109"/>
        <source>winstaller --- The installation will now abort.</source>
        <translation>سيتم إيقاف التثبيت الآن.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="110"/>
        <location filename="../../../../../extras/installer/win/strings.rc" line="114"/>
        <source>winstaller --- This is preventing the installation from proceeding.</source>
        <translation>هذا يمنع التثبيت من المتابعة.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="111"/>
        <source>winstaller --- Do you wish to continue the installation anyway?</source>
        <translation>هل ترغب في متابعة التثبيت على أي حال؟</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="112"/>
        <source>winstaller --- You can retry or attempt to continue the installation anyway.</source>
        <translation>يمكنك إعادة محاولة التثبيت أو محاولة استكماله على أي حال.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="113"/>
        <source>winstaller --- The uninstallation will now abort.</source>
        <translation>سيتم إيقاف إزالة التثبيت الآن.</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="115"/>
        <source>winstaller --- Do you wish to continue the uninstallation anyway?</source>
        <translation>هل ترغب في متابعة إزالة التثبيت على أي حال؟</translation>
    </message>
    <message>
        <location filename="../../../../../extras/installer/win/strings.rc" line="116"/>
        <source>winstaller --- You can retry or attempt to continue the uninstallation anyway.</source>
        <translation>يمكنك إعادة محاولة إزالة التثبيت أو محاولة استكماله على أي حال.</translation>
    </message>
</context>
</TS>