#ifndef DIALOG_DIALOG_STORE_H_
#define DIALOG_DIALOG_STORE_H_

#include <cstdint>

#include "exceptions.h"
#include "optional.h"
#include "storage.h"
#include "task_pool.h"
#include "atomic_multilog.h"
#include "file_utils.h"

namespace confluo {

class confluo_store {
 public:
  /**
   * Constructor for creating confluo store
   * @param data_path The data path for the store
   */
  confluo_store(const std::string& data_path)
      : data_path_(utils::file_utils::full_path(data_path)) {
    utils::file_utils::create_dir(data_path_);
  }

  /**
   * Adds a atomic multilog to confluo store
   *
   * @param name The name of the atomic multilog
   * @param schema List of columns that define the schema
   * @param id The storage mode of the atomic multilog
   * @return The id of the atomic multilog
   */
  int64_t create_atomic_multilog(const std::string& name,
                                 const std::vector<column_t>& schema,
                                 const storage::storage_id storage_type) {
    optional<management_exception> ex;
    storage::storage_mode mode = storage::STORAGE_MODES[storage_type];
    auto ret =
        mgmt_pool_.submit(
            [&name, &schema, &mode, &ex, this]() -> int64_t {
              size_t id;
              if (multilog_map_.get(name, id) != -1) {
                ex = management_exception("Table " + name + " already exists.");
                return INT64_C(-1);
              }
              utils::file_utils::create_dir(data_path_ + "/" + name);
              atomic_multilog* t = new atomic_multilog(name, schema, data_path_ + "/" + name,
                  mode, mgmt_pool_);
              id = atomic_multilogs_.push_back(t);
              if (multilog_map_.put(name, id) == -1) {
                ex = management_exception("Could not add atomic multilog " + name + " to atomic multilog map");
                return INT64_C(-1);
              }
              return id;
            });

    int64_t id = ret.get();
    if (ex.has_value())
      throw ex.value();
    return id;
  }

  /**
   * Gets the id of the atomic multilog
   * @param name The name of the atomic multilog
   * @return The id of the atomic multilog
   */
  int64_t get_atomic_multilog_id(const std::string& name) const {
    size_t id;
    if (multilog_map_.get(name, id) == -1) {
      throw management_exception("No such atomic multilog " + name);
    }
    return static_cast<int64_t>(id);
  }

  /**
   * Gets the specified atomic multilog
   * @param name The name of the atomic multilog
   * @return The atomic multilog that matches name
   */
  atomic_multilog* get_atomic_multilog(const std::string& name) {
    return atomic_multilogs_[get_atomic_multilog_id(name)];
  }

  /**
   * Gets the specified atomic multilog by id
   * @param id The id of the atomic multilog
   * @return The atomic multilog that matches the id
   */
  atomic_multilog* get_atomic_multilog(int64_t id) {
    if (id >= static_cast<int64_t>(atomic_multilogs_.size())) {
      throw management_exception("No such atomic multilog with id " + std::to_string(id));
    }
    return atomic_multilogs_[id];
  }

  /**
   * Removes a atomic multilog specified by the name
   * @param name The name of the atomic multilog
   * @return The index of the removed atomic multilog or -1 if it doesn't exist
   */
  int64_t remove_atomic_multilog(const std::string& name) {
    size_t id;
    if (multilog_map_.get(name, id) == -1) {
      throw management_exception("No such atomic multilog " + name);
    }
    return multilog_map_.remove(name, id);
  }

  /**
   * Removes the atomic multilog specified by the id
   * @param id The id of the atomic multilog
   * @return The index of the removed atomic multilog or -1 if it doesn't exist
   */
  int64_t remove_atomic_multilog(int64_t id) {
    return remove_atomic_multilog(get_atomic_multilog(id)->get_name());
  }

 private:
  // Metadata
  std::string data_path_;

  // Manangement
  task_pool mgmt_pool_;

  // Tables
  monolog::monolog_exp2<atomic_multilog*> atomic_multilogs_;
  string_map<size_t> multilog_map_;
};

}

#endif /* DIALOG_DIALOG_STORE_H_ */