//
// Created by sdk on 1/6/22.
//

#include <glib.h>
#include <libgen.h>
#include <dlfcn.h>
/* Nautilus extension headers */
#include <libnautilus-extension/nautilus-extension-types.h>
#include <libnautilus-extension/nautilus-file-info.h>
#include <libnautilus-extension/nautilus-info-provider.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-extension/nautilus-property-page-provider.h>

#include <iostream>

GType registeredExtensionType;

void nautilus_module_initialize(GTypeModule* module);

class QuickOpenNautilusExtension
{
    class TypeInfo
    {
        GObjectClass parent_slot;
    };

    GObject parent_slot;

public:
    static constexpr GTypeInfo getGTypeInfo()
    {
        return {
            sizeof(TypeInfo),
            nullptr, nullptr, // no base class
            nullptr, nullptr, nullptr, // no class initialization, finalization, or data functions
            sizeof(QuickOpenNautilusExtension),
            0,
            nullptr // no special instance initialization required
        };
    }
};

#define IF_ERROR(cond, action) if(cond) { \
    std::cerr << "ERROR: " #cond << " (" << __FILE__ << ", line " << __LINE__ << "; " \
              << "errno is " << errno << " (" << strerror(errno) << "))." << std::endl; \
              action; }

namespace QuickOpenMenuProvider
{
    static void onMenuItemActivated(NautilusMenuItem *item,
                             gpointer user_data)
    {
        auto* files = static_cast<GList *>(g_object_get_data(reinterpret_cast<GObject *>(item), "QuickOpenNautilusExtension_files"));
        IF_ERROR(files == nullptr, return);

        NautilusFileInfo* dirInfo = NAUTILUS_FILE_INFO(files->data);
        char* folderPath = g_file_get_path(nautilus_file_info_get_location(dirInfo));

        Dl_info libInfo;
        IF_ERROR(dladdr(reinterpret_cast<const void *>(nautilus_module_initialize), &libInfo) == 0, return);

        char* resolvedPath = realpath(libInfo.dli_fname, nullptr);
        IF_ERROR(resolvedPath == nullptr, return);

        std::string quickOpenPath = dirname(dirname(resolvedPath));
        quickOpenPath += "/bin/QuickOpen";

        free(resolvedPath);
        resolvedPath = nullptr;

        pid_t childPID = fork();
        IF_ERROR(childPID == -1, return);
        if(childPID == 0)
        {
            IF_ERROR(execl(quickOpenPath.c_str(), quickOpenPath.c_str(), "--set-default-upload-folder", folderPath, nullptr) == -1, exit(1));
        }
    }

    static GList* getFileItems(NautilusMenuProvider* provider,
                                                 GtkWidget* window,
                                                 GList* files)
    {
        // Only add the context menu item if a single directory is selected
        if(files == nullptr || files->next != nullptr || !nautilus_file_info_is_directory(NAUTILUS_FILE_INFO(files->data)))
        {
            return nullptr;
        }

        NautilusMenuItem* item = nautilus_menu_item_new ("QuickOpenNautilusExtension::set_default_destination",
                                       "Set Default QuickOpen Destination",
                                       "Set the default destination for QuickOpen file uploads",
                                       nullptr /* icon name */);
        g_signal_connect (item, "activate", G_CALLBACK (onMenuItemActivated), provider);
        g_object_set_data_full ((GObject*) item, "QuickOpenNautilusExtension_files",
                                nautilus_file_info_list_copy (files),
                                (GDestroyNotify)nautilus_file_info_list_free);
        return g_list_append (nullptr, item);
    }

    static void initializeInterface(NautilusMenuProviderIface* iface)
    {
        iface->get_file_items = getFileItems;
    }
}

void nautilus_module_initialize(GTypeModule* module)
{
    static const GTypeInfo info = QuickOpenNautilusExtension::getGTypeInfo();

    static const GInterfaceInfo menu_provider_iface_info = {
            reinterpret_cast<GInterfaceInitFunc>(QuickOpenMenuProvider::initializeInterface),
            nullptr, // no interface finalization
            nullptr // no interface data
    };

    registeredExtensionType = g_type_module_register_type(module,
                                                           G_TYPE_OBJECT,
                                                           "QuickOpenExtension",
                                                           &info, static_cast<GTypeFlags>(0));

    g_type_module_add_interface (module,
                                 registeredExtensionType,
                                 NAUTILUS_TYPE_MENU_PROVIDER,
                                 &menu_provider_iface_info);
}

void nautilus_module_shutdown(void)
{
    /* Any module-specific shutdown */
    std::cout << "Module is shutting down." << std::endl;
}

void nautilus_module_list_types(const GType** types, int* num_types)
{
    static GType typeInfoArray[] = { registeredExtensionType };

    *types = typeInfoArray;
    *num_types = G_N_ELEMENTS(typeInfoArray);
}