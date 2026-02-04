#include <SDK/foobar2000.h>

class library_enumerator : public initquit {
public:
    virtual ~library_enumerator() = default;

    void on_init() override {
        metadb_handle_list items;
        static_api_ptr_t<library_manager>()->get_all_items(items);

        for (t_size i = 0; i < items.get_count(); ++i) {
            auto* path = items[i]->get_path();
            console::print(path);
        }
    }
};

static initquit_factory_t<library_enumerator> g_library_enumerator;

DECLARE_COMPONENT_VERSION(
  "foo_ai",
  "0.0.1",
  "AI functions for foobar.\n"
  "See http://yirkha.fud.cz/progs/foobar2000/tutorial.php"
);