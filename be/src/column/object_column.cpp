// This file is licensed under the Elastic License 2.0. Copyright 2021-present, StarRocks Limited.

#include "column/object_column.h"

#include "gutil/casts.h"
#include "types/bitmap_value.h"
#include "types/hll.h"
#include "util/json.h"
#include "util/mysql_row_buffer.h"
#include "util/percentile_value.h"

namespace starrocks::vectorized {

template <typename T>
size_t ObjectColumn<T>::byte_size(size_t from, size_t size) const {
    DCHECK_LE(from + size, this->size()) << "Range error";
    size_t byte_size = 0;
    for (int i = 0; i < size; ++i) {
        byte_size += _pool[from + i].serialize_size();
    }
    return byte_size;
}

template <typename T>
size_t ObjectColumn<T>::byte_size(size_t idx) const {
    DCHECK(false) << "Don't support object column byte size";
    return 0;
}

template <typename T>
void ObjectColumn<T>::assign(size_t n, size_t idx) {
    if (idx != 0) {
        _pool[0] = std::move(_pool[idx]);
    }
    _pool.resize(1);
    _pool.reserve(n);

    for (int i = 1; i < n; ++i) {
        append(&_pool[0]);
    }

    _cache_ok = false;
}

template <typename T>
void ObjectColumn<T>::append(const T* object) {
    _pool.emplace_back(*object);
    _cache_ok = false;
}

template <typename T>
void ObjectColumn<T>::append(T&& object) {
    _pool.emplace_back(std::move(object));
    _cache_ok = false;
}

template <typename T>
void ObjectColumn<T>::remove_first_n_values(size_t count) {
    size_t remain_size = _pool.size() - count;
    for (int i = 0; i < remain_size; ++i) {
        _pool[i] = std::move(_pool[count + i]);
    }

    _pool.resize(remain_size);
    _cache_ok = false;
}

template <typename T>
void ObjectColumn<T>::append(const Column& src, size_t offset, size_t count) {
    const auto& obj_col = down_cast<const ObjectColumn<T>&>(src);
    for (int i = offset; i < count + offset; ++i) {
        append(obj_col.get_object(i));
    }
}

template <typename T>
void ObjectColumn<T>::append_selective(const starrocks::vectorized::Column& src, const uint32_t* indexes, uint32_t from,
                                       uint32_t size) {
    const auto& obj_col = down_cast<const ObjectColumn<T>&>(src);
    for (int j = 0; j < size; ++j) {
        append(obj_col.get_object(indexes[from + j]));
    }
};

template <typename T>
void ObjectColumn<T>::append_value_multiple_times(const starrocks::vectorized::Column& src, uint32_t index,
                                                  uint32_t size) {
    const auto& obj_col = down_cast<const ObjectColumn<T>&>(src);
    for (int j = 0; j < size; ++j) {
        append(obj_col.get_object(index));
    }
};

template <typename T>
bool ObjectColumn<T>::append_strings(const Buffer<starrocks::Slice>& strs) {
    _pool.reserve(_pool.size() + strs.size());
    for (const Slice& s : strs) {
        _pool.emplace_back(s);
    }

    _cache_ok = false;
    return true;
}

template <typename T>
void ObjectColumn<T>::append_value_multiple_times(const void* value, size_t count) {
    const Slice* slice = reinterpret_cast<const Slice*>(value);
    _pool.reserve(_pool.size() + count);

    for (int i = 0; i < count; ++i) {
        _pool.emplace_back(*reinterpret_cast<T*>(slice->data));
    }

    _cache_ok = false;
};

template <typename T>
void ObjectColumn<T>::append_default() {
    _pool.emplace_back(T());
    _cache_ok = false;
}

template <typename T>
void ObjectColumn<T>::append_default(size_t count) {
    for (int i = 0; i < count; ++i) {
        append_default();
    }
}

template <typename T>
void ObjectColumn<T>::fill_default(const Filter& filter) {
    for (size_t i = 0; i < filter.size(); i++) {
        if (filter[i] == 1) {
            _pool[i] = {};
        }
    }
    _cache_ok = false;
}

template <typename T>
Status ObjectColumn<T>::update_rows(const Column& src, const uint32_t* indexes) {
    const auto& obj_col = down_cast<const ObjectColumn<T>&>(src);
    size_t replace_num = src.size();
    for (size_t i = 0; i < replace_num; i++) {
        DCHECK_LT(indexes[i], _pool.size());
        _pool[indexes[i]] = *obj_col.get_object(i);
    }
    _cache_ok = false;
    return Status::OK();
}

template <typename T>
uint32_t ObjectColumn<T>::serialize(size_t idx, uint8_t* pos) {
    DCHECK(false) << "Don't support object column serialize";
    return 0;
}

template <typename T>
uint32_t ObjectColumn<T>::serialize_default(uint8_t* pos) {
    DCHECK(false) << "Don't support object column serialize";
    return 0;
}

template <typename T>
void ObjectColumn<T>::serialize_batch(uint8_t* dst, Buffer<uint32_t>& slice_sizes, size_t chunk_size,
                                      uint32_t max_one_row_size) {
    DCHECK(false) << "Don't support object column serialize batch";
}

template <typename T>
const uint8_t* ObjectColumn<T>::deserialize_and_append(const uint8_t* pos) {
    DCHECK(false) << "Don't support object column deserialize and append";
    return pos;
}

template <typename T>
void ObjectColumn<T>::deserialize_and_append_batch(Buffer<Slice>& srcs, size_t chunk_size) {
    DCHECK(false) << "Don't support object column deserialize and append";
}

template <typename T>
uint32_t ObjectColumn<T>::serialize_size(size_t idx) const {
    DCHECK(false) << "Don't support object column byte size";
    return 0;
}

template <typename T>
size_t ObjectColumn<T>::filter_range(const Column::Filter& filter, size_t from, size_t to) {
    size_t old_sz = size();
    size_t new_sz = from;
    for (auto i = from; i < to; ++i) {
        if (filter[i]) {
            std::swap(_pool[new_sz], _pool[i]);
            new_sz++;
        }
    }
    DCHECK_LE(new_sz, to);
    if (new_sz < to) {
        for (int i = to; i < old_sz; i++) {
            std::swap(_pool[new_sz], _pool[i]);
            new_sz++;
        }
    }
    _pool.resize(new_sz);
    return new_sz;
}

template <typename T>
int ObjectColumn<T>::compare_at(size_t left, size_t right, const starrocks::vectorized::Column& rhs,
                                int nan_direction_hint) const {
    DCHECK(false) << "Don't support object column compare_at";
    return 0;
}

template <typename T>
void ObjectColumn<T>::fnv_hash(uint32_t* hash, uint32_t from, uint32_t to) const {
    std::string s;
    for (int i = from; i < to; ++i) {
        s.resize(_pool[i].serialize_size());
        int32_t size = _pool[i].serialize(reinterpret_cast<uint8_t*>(s.data()));
        hash[i] = HashUtil::fnv_hash(s.data(), size, hash[i]);
    }
}

template <typename T>
void ObjectColumn<T>::crc32_hash(uint32_t* hash, uint32_t from, uint32_t to) const {
    DCHECK(false) << "object column shouldn't call crc32_hash ";
}

template <typename T>
int64_t ObjectColumn<T>::xor_checksum(uint32_t from, uint32_t to) const {
    DCHECK(false) << "object column shouldn't call xor_checksum";
    return 0;
}

template <typename T>
void ObjectColumn<T>::put_mysql_row_buffer(starrocks::MysqlRowBuffer* buf, size_t idx) const {
    buf->push_null();
}

template <typename T>
void ObjectColumn<T>::_build_slices() const {
    // TODO(kks): improve this
    _buffer.clear();
    _slices.clear();

    // FIXME(kks): bitmap itself compress is more effective than LZ4 compress?
    // Do we really need compress bitmap here?
    if constexpr (std::is_same_v<T, BitmapValue>) {
        for (int i = 0; i < _pool.size(); ++i) {
            _pool[i].compress();
        }
    }

    size_t size = byte_size();
    _buffer.resize(size);
    _slices.reserve(_pool.size());
    size_t old_size = 0;
    for (int i = 0; i < _pool.size(); ++i) {
        int32_t slice_size = _pool[i].serialize(_buffer.data() + old_size);
        _slices.emplace_back(Slice(_buffer.data() + old_size, slice_size));
        old_size += slice_size;
    }
}

template <typename T>
MutableColumnPtr ObjectColumn<T>::clone() const {
    auto p = clone_empty();
    p->append(*this, 0, size());
    return p;
}

template <typename T>
ColumnPtr ObjectColumn<T>::clone_shared() const {
    auto p = clone_empty();
    p->append(*this, 0, size());
    return p;
}

template <typename T>
std::string ObjectColumn<T>::debug_item(uint32_t idx) const {
    return "";
}

template <>
std::string ObjectColumn<BitmapValue>::debug_item(uint32_t idx) const {
    return _pool[idx].to_string();
}

template <typename T>
StatusOr<ColumnPtr> ObjectColumn<T>::upgrade_if_overflow() {
    if (reach_capacity_limit()) {
        return Status::InternalError("Size of ObjectColumn exceed the limit");
    }
    return nullptr;
}

template class ObjectColumn<HyperLogLog>;
template class ObjectColumn<BitmapValue>;
template class ObjectColumn<PercentileValue>;
template class ObjectColumn<JsonValue>;

} // namespace starrocks::vectorized
